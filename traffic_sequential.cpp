#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <map>
#include <chrono>

using namespace std;

// itll give our traffic record a good shape.
struct TrafficRecord {
    string timestamp, lightID;
    int cars;
};

// stores the total traffic for every light in every hour
map<string, map<string, long long>> hourlyTraffic;

int main(int argc, char *argv[])
{
    if(argc != 3)
    {
        cout << "Usage: ./traffic_sequential <traffic_file> <topN>" << endl;
        return 1;
    }

    string fileName=argv[1];

    int topN;
    try
    {
        topN=stoi(argv[2]); 
    }
    catch (...)
    {
        cout << "Top N must be a number." << endl;
        return 1;
    }

    if(topN<=0)
    {
        cout << "Top N MUST be over 0!" << endl;
        return 1;
    }

    ifstream inputFile(fileName);
    if(!inputFile.is_open())
    {
        cout << "FAILED to open " << fileName << endl;
        return 1;
    }

    hourlyTraffic.clear();
    long long recordsProcessed=0;

    cout << "\n - TRAFFIC SIMULATOR (Sequential) -" << endl;
    cout << " " << endl;
    cout << "Input file: " << fileName << endl;
    cout << "Top N: " << topN << endl;
    cout << " " << endl;

    auto start=chrono::high_resolution_clock::now();
    string line;
    while(getline(inputFile, line))
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
        hourlyTraffic[hour][record.lightID]+=record.cars;
        recordsProcessed++;
    }

    inputFile.close();
    // stores our final sorted results for every hour
    map<string, vector<pair<string, long long>>> sortedTraffic;

    for(auto &hourEntry:hourlyTraffic)
    {
        vector<pair<string, long long>> trafficLights;
        for(auto &lightEntry:hourEntry.second)
            trafficLights.push_back(lightEntry);

        sort(
            trafficLights.begin(),
            trafficLights.end(),
            [](const auto &a, const auto &b)
            {
                if(a.second==b.second)
                    return a.first < b.first;

                return a.second > b.second;
            }
        );
        sortedTraffic[hourEntry.first]=trafficLights;
    }

    auto end=chrono::high_resolution_clock::now();
    chrono::duration<double> elapsed=end-start;


    cout << "\n - Simulation Summary - " << endl;
    cout << "" << endl;
    cout << "Input File: " << fileName << endl;
    cout << "Processed Records: " << recordsProcessed << endl;
    cout << "Execution Time: " << elapsed.count() << " seconds" << endl;


    //after the traffic has been processed, print the top congested lights
    for(auto &hourEntry:sortedTraffic)
    {
        string hour=hourEntry.first;
        vector<pair<string, long long>> &trafficLights=hourEntry.second;

        cout << "\nHour: " << hour << ":00" << endl;
        cout << "Top " << topN << " congested traffic lights: " << endl;

        int limit=min(topN, (int)trafficLights.size());
        for(int i=0; i<limit; i++)
        {
            cout << i+1 << ". "
            << trafficLights[i].first
            << " - "
            << trafficLights[i].second
            << " cars"
            << endl;
        }
    }

    return 0;
}