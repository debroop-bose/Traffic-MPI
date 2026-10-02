#include <iostream>
#include <fstream>
#include <random>
#include <iomanip>
#include <string>

using namespace std;

const int SIMULATION_HOURS=24;

// generates normal traffic data
bool generateTrafficData(string fileName, int numSignals)
{
    ofstream outputFile(fileName);

    if (!outputFile.is_open())
    {
        cout << fileName << " could NOT be created!" << endl;
        return false;
    }

    // generates the same data every time for comparison
    mt19937 generator(42);

    // number of cars for one five minute reading
    uniform_int_distribution<int> carDistri(1, 60);

    long long records=0;

    for(int hour=0; hour<SIMULATION_HOURS; hour++)
    {
        for(int min=0; min<60; min+=5)
        {
            for(int signal=1; signal<=numSignals; signal++)
            {
                int cars=carDistri(generator);

                outputFile
                << setw(2) << setfill('0') << hour
                << ":"
                << setw(2) << setfill('0') << min
                << ",TL" << signal
                << "," << cars
                << '\n';

                records++;
            }
        }
    }

    outputFile.close();

    cout << fileName << " created successfully." << endl;
    cout << "Traffic signals: " << numSignals << endl;
    cout << "Records: " << records << endl;

    return true;
}


// generates uneven data for the Level 3 test
bool generateUnevenTrafficData(string fileName, int numSignals)
{
    ofstream outputFile(fileName);

    if (!outputFile.is_open())
    {
        cout << fileName << " could NOT be created!" << endl;
        return false;
    }

    mt19937 generator(42);

    uniform_int_distribution<int> carDistri(1, 60);

    long long records=0;

    // first create the normal traffic records
    for(int hour=0; hour<SIMULATION_HOURS; hour++)
    {
        for(int min=0; min<60; min+=5)
        {
            for(int signal=1; signal<=numSignals; signal++)
            {
                int cars=carDistri(generator);

                outputFile
                << setw(2) << setfill('0') << hour
                << ":"
                << setw(2) << setfill('0') << min
                << ",TL" << signal
                << "," << cars
                << '\n';

                records++;
            }
        }
    }

    // extra records deliberately make the total uneven
    // most of them also belong to TL1 so that one key is much more common
    const int EXTRA_RECORDS=50003;

    for(int i=0; i<EXTRA_RECORDS; i++)
    {
        int hour=i % SIMULATION_HOURS;
        int min=(i % 12) * 5;

        string lightID;

        if(i % 5 != 0)
        {
            lightID="TL1";
        }
        else
        {
            lightID="TL2";
        }

        int cars=carDistri(generator);

        outputFile
        << setw(2) << setfill('0') << hour
        << ":"
        << setw(2) << setfill('0') << min
        << "," << lightID
        << "," << cars
        << '\n';

        records++;
    }

    outputFile.close();

    cout << fileName << " created successfully." << endl;
    cout << "Traffic signals: " << numSignals << endl;
    cout << "Extra uneven records: " << EXTRA_RECORDS << endl;
    cout << "Total records: " << records << endl;

    return true;
}


int main()
{
    cout << "Generating performance datasets..." << endl << endl;

    // three clear workload sizes
    if (!generateTrafficData("traffic_medium.txt", 10000))
    {
        return 1;
    }

    cout << endl;

    if (!generateTrafficData("traffic_large.txt", 20000))
    {
        return 1;
    }

    cout << endl;

    if (!generateTrafficData("traffic_very_large.txt", 30000))
    {
        return 1;
    }

    cout << endl;

    cout << endl;

    // stress test dataset
    if (!generateTrafficData("traffic_stress.txt", 500000))
    {
        return 1;
    }

    // Level 3 hard case
    if (!generateUnevenTrafficData("traffic_uneven.txt", 20000))
    {
        return 1;
    }

    cout << "\nAll test data files were generated successfully." << endl;

    return 0;
}