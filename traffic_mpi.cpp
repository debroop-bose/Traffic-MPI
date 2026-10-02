#include <mpi.h>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <map>
#include <iomanip>
#include <climits>

using namespace std;

// itll give our traffic record a structure
struct TrafficRecord {
    string timestamp, lightID;
    int cars;
};

// this decide which mpi process will reduce a particular key
int getReducer(string hour, string lightID, int numProcesses)
{
    string key = hour + "|" + lightID;
    unsigned long long hashVal=0;
    for (char c:key)
        hashVal=hashVal * 31 + static_cast<unsigned char>(c);

    return static_cast<int>(hashVal % numProcesses);
}

int main (int argc, char *argv[])
{
    MPI_Init(&argc, &argv);
    int rank, numProcesses;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &numProcesses);
    
    if(argc != 3)
    {
        if (rank==0)
        
            // it'll take something like ./taffic_mpi traffic_uneven 3
            cout << "Usage: ./traffic_mpi <traffic_file> <topN>" << endl;
        
        MPI_Finalize();
        return 1;
    }

    string fileName=argv[1];
    int topN;

    try
    {
        topN=stoi(argv[2]);
    }
    catch(...)
    {
        if(rank==0)
            cout << "Top N must be a NUMBER! This is a wrong input." << endl;

        MPI_Finalize();
        return 1;
    }


    if(topN<=0)
    {
        if(rank==0)
            cout << "The Top N MUST be over 0!" << endl;

        MPI_Finalize();
        return 1;
    }

    ifstream inputFile;
    int fileOK=1;

    if(rank==0)
    {
        inputFile.open(fileName);

        if(!inputFile.is_open())
        {
            cout << "FAILED to open " << fileName << endl;
            fileOK=0;
        }
    }

    MPI_Bcast(&fileOK, 1, MPI_INT, 0, MPI_COMM_WORLD);

    if(fileOK==0)
    {
        MPI_Finalize();
        return 1;
    }

    MPI_Barrier(MPI_COMM_WORLD);

    double start=MPI_Wtime();

    // this is a master process that reads all the traffic file
    vector <string> everyLines;
    if (rank==0)
    {
        string line;
        while (getline(inputFile, line))
        {
            // this handles an edge case
            if(line.empty())
                continue;

            everyLines.push_back(line);
        }
        inputFile.close();
    }
    long long totalRecs=0;
    if (rank==0)
        totalRecs=static_cast<long long>(everyLines.size());
    
    MPI_Bcast(&totalRecs, 1, MPI_LONG_LONG,0,MPI_COMM_WORLD);

    // this handles another edge case
    if (totalRecs==0)
    {
        if (rank==0)
            cout << "The input file has no records of traffic!" << endl;
        
        MPI_Finalize();
        return 1;
    }

    // this divides the records as evenly as possible among all the mpi processes
    long long normalSize=totalRecs / numProcesses;
    long long extraRecs=totalRecs % numProcesses; // calculates any extra records for uneven number of records

    long long localExpectedRecs=normalSize;
    if (rank < extraRecs)
        localExpectedRecs++;

    vector<string> rankData(numProcesses);
    vector<int> sendCounts(numProcesses, 0), displs(numProcesses, 0);
    string sendData;

    // the process 0 will divide the file between the mpi processes
    if (rank==0)
    {
        long long lineInd=0; // index
        for (int process=0; process<numProcesses; process++)
        {
            long long recordsForProcess=normalSize;

            if(process < extraRecs)
                recordsForProcess++;
        
            for (long long i=0; i<recordsForProcess; i++)
            {
                rankData[process]+=everyLines[lineInd];
                rankData[process]+='\n';
                lineInd++;
            }

            if(rankData[process].size() > INT_MAX)
            {
                cout << "A MPI partition is too large for MPI_Scatterv." << endl;
                MPI_Abort(MPI_COMM_WORLD, 1);
            }
            sendCounts[process]=static_cast<int>(rankData[process].size());
        }

        for (int process=1; process<numProcesses; process++)
            displs[process]=displs[process - 1]+sendCounts[process - 1];

        long long totalBytes=0;

        for (int process=0; process<numProcesses; process++)
            totalBytes+=sendCounts[process];

        if(totalBytes > INT_MAX)
        {
            cout << "The complete traffic file is too large for this MPI_Scatterv test." << endl;
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        sendData.reserve(static_cast<size_t>(totalBytes));
        for (int process=0; process < numProcesses; process++)
            sendData+=rankData[process];
    }

    int recvCount=0;
    MPI_Scatter(sendCounts.data(), 1, MPI_INT, &recvCount, 1, MPI_INT, 0, MPI_COMM_WORLD);
    vector <char> recvBuffer(recvCount);

    MPI_Scatterv(rank==0 ? sendData.data() : nullptr, sendCounts.data(), displs.data(), MPI_CHAR, recvBuffer.data(), recvCount, MPI_CHAR, 0, MPI_COMM_WORLD);
    string recvData(recvBuffer.begin(), recvBuffer.end());

    // we clear this as process 0 doesn't need the complete file anymore
    if (rank==0)
    {
        everyLines.clear();
        rankData.clear();
        sendData.clear();
    }

    // MAP
    // every process reads only its own section and creates the local key value totals
    map <string, map<string, long long>> hourlyTraffic;
    long long recordsProcessed=0;
    stringstream inputStream(recvData);
    string line;

    while(getline(inputStream, line))
    {
        if(line.empty())
            continue;

        string timestamp, lightID, carsText;
        stringstream ss(line);

        getline(ss, timestamp, ',');
        getline(ss, lightID, ',');
        getline(ss, carsText, ',');

        if(timestamp.empty() || lightID.empty() || carsText.empty())
            continue;


        TrafficRecord record;

        record.timestamp=timestamp;
        record.lightID=lightID;

        try
        {
            record.cars=stoi(carsText);
        }
        catch (...)
        {
            continue;
        }

        if(record.timestamp.size() < 2)
            continue;

        string hour=record.timestamp.substr(0,2);

        // local combination happens here
        //key = hour + lightID
        // value = total cars
        hourlyTraffic[hour][record.lightID]+=record.cars;
        recordsProcessed++;

    }
    recvData.clear();
    recvBuffer.clear();

    // this collects some info about the map step for clear output
    long long localMappedKeys=0;
    for (auto &hourEntry:hourlyTraffic)
        localMappedKeys+=hourEntry.second.size();
    
    vector<long long> rankRecCounts(numProcesses, 0), rankKeyCounts(numProcesses, 0);

    MPI_Gather(&recordsProcessed, 1, MPI_LONG_LONG, rankRecCounts.data(), 1, MPI_LONG_LONG, 0, MPI_COMM_WORLD);
    MPI_Gather(&localMappedKeys,1, MPI_LONG_LONG, rankKeyCounts.data(), 1, MPI_LONG_LONG, 0, MPI_COMM_WORLD);

    // SHUFFLE
    // each combined key is sent to 1 reducer process
    vector <string> shuffleParts(numProcesses);

    for (auto &hourEntry : hourlyTraffic)
    {
        string hour=hourEntry.first;
        for (auto &lightEntry:hourEntry.second)
        {
            string lightID=lightEntry.first;
            long long cars=lightEntry.second;

            int reducer=getReducer(hour, lightID, numProcesses);

            shuffleParts[reducer]+=hour;
            shuffleParts[reducer]+=",";
            shuffleParts[reducer]+=lightID;
            shuffleParts[reducer]+=",";
            shuffleParts[reducer]+=to_string(cars);
            shuffleParts[reducer]+='\n';

        }
    }

    // we have converted the local map into shuffle data
    hourlyTraffic.clear();

    vector<int> shuffleSendCounts(numProcesses, 0);
    vector<int> shuffleRecvCounts(numProcesses, 0);
    vector<int> shuffleSendDispls(numProcesses, 0);
    vector<int> shuffleRecvDispls(numProcesses, 0);
    string shuffleSendData;

    for (int process=0; process<numProcesses; process++)
    {
        if(shuffleParts[process].size() > INT_MAX)
        {
            cout << "Rank " << rank << " has too much shuffle data." << endl;
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        shuffleSendCounts[process]=static_cast<int>(shuffleParts[process].size());
    }

    for (int process=1; process<numProcesses; process++)
        shuffleSendDispls[process] = shuffleSendDispls[process - 1] + shuffleSendCounts[process - 1];

    for (int process=0; process<numProcesses; process++)
        shuffleSendData+=shuffleParts[process];
    
    // first exchange how much data every rank will receive
    MPI_Alltoall(shuffleSendCounts.data(), 1, MPI_INT, shuffleRecvCounts.data(), 1, MPI_INT,MPI_COMM_WORLD);
    
    int totalShuffleRecv=0;
    for (int process=0; process < numProcesses; process++)
    {
        if (process>0)
        {
            shuffleRecvDispls[process]=shuffleRecvDispls[process - 1] + shuffleRecvCounts[process - 1];
        }

        totalShuffleRecv+=shuffleRecvCounts[process];
    }
    vector <char> shuffleRecvBuffer(totalShuffleRecv);

    // exchange the actual map output between reducer process
    MPI_Alltoallv(shuffleSendData.data(), shuffleSendCounts.data(), 
    shuffleSendDispls.data(), MPI_CHAR, shuffleRecvBuffer.data(), shuffleRecvCounts.data(), shuffleRecvDispls.data(), MPI_CHAR, MPI_COMM_WORLD);

    string shuffleRecvData(shuffleRecvBuffer.begin(), shuffleRecvBuffer.end());
    shuffleParts.clear();
    shuffleSendData.clear();
    shuffleRecvBuffer.clear();

    // Reduce
    // each rank adds together every partial total for the keys assigned to it
    map <string, map<string, long long>> reduceTraffic;
    stringstream reduceStream(shuffleRecvData);

    while (getline(reduceStream, line))
    {
        if(line.empty())
            continue;
        
        string hour, lightID, carsText;
        stringstream ss(line);
        getline(ss, hour, ',');
        getline(ss, lightID, ',');
        getline(ss, carsText, ',');

        if(hour.empty() || lightID.empty() || carsText.empty())
            continue;
        
        try
        {
            long long cars=stoll(carsText);
            reduceTraffic[hour][lightID]+=cars;
        }
        catch(...)
        {
            continue;
        }
    }
    shuffleRecvData.clear();

    long long localReducedKeys=0;
    for(auto &hourEntry:reduceTraffic)
        localReducedKeys+=hourEntry.second.size();

    long long localShuffleBytes=totalShuffleRecv;

    vector<long long> rankReduceKeyCounts(numProcesses, 0);
    vector<long long> rankShuffleBytes(numProcesses, 0);

    MPI_Gather(&localReducedKeys, 1, MPI_LONG_LONG, rankReduceKeyCounts.data(), 1, MPI_LONG_LONG, 0, MPI_COMM_WORLD);
    MPI_Gather(&localShuffleBytes, 1, MPI_LONG_LONG, rankShuffleBytes.data(), 1, MPI_LONG_LONG, 0, MPI_COMM_WORLD);

    // each reducer only sends its own top n candidates for every hour
    // this avoids sending all the final traffic lights back to process 0

    string candidateData;
    for(auto &hourEntry:reduceTraffic)
    {
        string hour=hourEntry.first;
        vector<pair<string, long long>> trafficLights;

        for(auto &lightEntry:hourEntry.second)
            trafficLights.push_back(lightEntry);
    
        sort(trafficLights.begin(), trafficLights.end(),
            [](const auto &a, const auto &b)
            {
                if(a.second==b.second)
                    return a.first < b.first;

                return a.second > b.second;
            }
        );

        int limit=min(topN, (int)trafficLights.size());

        for (int i=0; i<limit; i++)
        {
            candidateData+=hour;
            candidateData+=",";
            candidateData+=trafficLights[i].first;
            candidateData+=",";
            candidateData+=to_string(trafficLights[i].second);
            candidateData+='\n';
        }
    }

    int candidateSize=static_cast<int>(candidateData.size());
    vector<int> candidateCounts(numProcesses, 0);
    vector<int> candidateDispls(numProcesses, 0);

    MPI_Gather(&candidateSize,1,MPI_INT, candidateCounts.data(), 1, MPI_INT, 0, MPI_COMM_WORLD);
    int totalCandidateSize=0;

    if (rank==0)
    {
        for(int process=0; process<numProcesses; process++)
        {
            if(process>0)
            {
                candidateDispls[process]=candidateDispls[process-1] + candidateCounts[process-1];  
            }
            totalCandidateSize+=candidateCounts[process];
        }
    }

    vector<char> finalBuffer;
    if(rank==0)
        finalBuffer.resize(totalCandidateSize);

    MPI_Gatherv(candidateData.data(), candidateSize, MPI_CHAR, finalBuffer.data(), candidateCounts.data(), candidateDispls.data(), MPI_CHAR, 0, MPI_COMM_WORLD);

    // process 0 finds the final global top N
    map<string, map<string, long long>> finalTraffic;
    map<string, vector<pair<string, long long>>> sortedTraffic;

    if (rank==0)
    {
        string finalData(finalBuffer.begin(), finalBuffer.end());
        stringstream finalStream(finalData);

        while(getline(finalStream, line))
        {
            if(line.empty())
                continue;

            string hour, lightID, carsText;
            stringstream ss(line);
            getline(ss, hour, ',');
            getline(ss, lightID, ',');
            getline(ss, carsText, ',');

            if(hour.empty() || lightID.empty() || carsText.empty())
                continue;

            try
            {
                long long cars=stoll(carsText);
                finalTraffic[hour][lightID]+=cars;
            }
            catch (...)
            {
                continue;
            }
        }

        for (auto &hourEntry:finalTraffic)
        {
            vector<pair<string, long long>> trafficLights;

            for (auto &lightEntry:hourEntry.second)
                trafficLights.push_back(lightEntry);
            
            sort(trafficLights.begin(), trafficLights.end(),
            [](const auto &a, const auto &b)
            {
                if(a.second==b.second)
                    return a.first < b.first;

                return a.second > b.second;
            });

            sortedTraffic[hourEntry.first]=trafficLights;
        }
    }

    // add all processed record counts so process 0 can check its correctness
    long long totalProcessed=0;
    MPI_Reduce(&recordsProcessed, &totalProcessed, 1,MPI_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
    double end = MPI_Wtime();

    double localElapsed = end - start;
    double elapsed=0;

    MPI_Reduce(&localElapsed, &elapsed, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if (rank==0)
    {
        cout << "\n - Traffic Simulator (MPI Version) -" << endl;
        cout << " " << endl;

        cout << "Input File: " << fileName << endl;
        cout << "Top N: " << topN << endl;
        cout << "MPI Processes: " << numProcesses << endl;
        cout << "Total Input Records: " << totalRecs << endl;
        cout << "Processed Records: " << totalProcessed << endl;
        cout << "Execution Time: " << elapsed << endl;

        if(totalProcessed==totalRecs)
            cout << "Record check: PASSED. We have processed all the records!" << endl;
        else    
            cout << "Record check: FAILED. The record count doesn't match!" << endl;

        cout << "\n - Input Split and Map Results -" << endl;
        for (int process=0; process<numProcesses; process++)
        {
            long long expectedForProcess=normalSize;
            if (process < extraRecs)
                expectedForProcess++;

            cout << "Rank " << process << " received " << expectedForProcess << " records, processed " << rankRecCounts[process]
            << " records and created " << rankKeyCounts[process] << " local keys." << endl;
        }

        cout << "\n - Shuffle and Reduce Results -" << endl;
        for(int process=0; process<numProcesses; process++)
        {
            cout << "Rank " << process
            << " received " << rankShuffleBytes[process]
            << " shuffle bytes and reduced "
            << rankReduceKeyCounts[process]
            << " keys."
            << endl;
        }

        //after the traffic has been processed we print the top congested lights
        for(auto &hourEntry:sortedTraffic)
        {
            string hour=hourEntry.first;
            vector <pair<string, long long>> &trafficLights=hourEntry.second;
            cout << "\n Hour: " << hour << ":00" << endl;
    
            cout << "Top " << topN << " congested traffic lights: " << endl;

            int limit=min(topN, (int)trafficLights.size());

            for (int i=0; i<limit; i++)
            {
                cout << i+1 << ". " << trafficLights[i].first << " - " << trafficLights[i].second << " cars" << endl;
            }
        }
    }

    MPI_Finalize();
    return 0;

}