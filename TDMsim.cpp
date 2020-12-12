// g++ -O5 -g -o TDMSim TDMsim.cpp && ./TDMSim

#include <stdlib.h>
#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <random>
#include <queue>
#include <string>

#define MIN(a,b) ((a<b)?(a):(b))
#define MAX(a,b) ((a>b)?(a):(b))

/******************************************************************************
 * CONTROL START
 *****************************************************************************/

// average (true) or max (false)
#define latencyType true

// bitcomp/tornado (true) or uniform(false)
#define trafficType false

// Inject simultaneously packets
#define packets 1

// 1 -> Print for plots
// 2 -> Print for observation
// 3 -> Print everything for observation
#define OutputMode 3

// Select the type of network to consider - if _MESH is undefined, bitorus is assumed
//#define _MESH
#define _NODES_ON_EDGE 8   // Pick one of 8 or 15
#define _PACKET_SIZE 3     // Pick one of 3 or 17

// Select whether to simulate a constant number of packets per IR or a specific number
// of TDM cycle ticks
#define _CONSTANT_NUM_PACKETS 100000

// Add the hop delay (pipeline depth) added by the NoC
#define _LINK_DELAY 0
#define _ROUTER_DELAY 3

// The TDM cycle length in clock cycles
#define _TDM_SLOT_LENGTH 3

// TDM period and end IRs
#define _NODES (_NODES_ON_EDGE * _NODES_ON_EDGE - 1)
#define _IR_START 0.001
#define _IR_STEP 0.005
#ifdef _MESH
    #if _NODES_ON_EDGE == 8
        #if _PACKET_SIZE == 3 
            #if trafficType
                #define _TDM_PERIOD 14
            #else
                #define _TDM_PERIOD 139
            #endif
        #else
            #define _TDM_PERIOD 1328
        #endif
    #else
        #if _PACKET_SIZE == 3
            #define _TDM_PERIOD 886
        #else
            #define _TDM_PERIOD 8350
        #endif
    #endif
#else
    #if _NODES_ON_EDGE == 8
        #if _PACKET_SIZE == 3 
            #if trafficType
                #define _TDM_PERIOD 6
            #else
                #define _TDM_PERIOD 85
            #endif
        #else
            #define _TDM_PERIOD 831
        #endif
    #else
        #if _PACKET_SIZE == 3
            #define _TDM_PERIOD 471
        #else
            #define _TDM_PERIOD 4833
        #endif
    #endif
#endif
#if trafficType
    #define _IR_END (1.0 / _TDM_PERIOD)
#else
    #define _IR_END ((double) (_NODES * _PACKET_SIZE) / (_TDM_PERIOD * _TDM_SLOT_LENGTH))
#endif

/******************************************************************************
 * CONTROL END
 *****************************************************************************/
 
using namespace std;

double AverageHops(int N);
int MaximumHops(int N);

int main()
{
	// Setup the destination node and the injection rate
    std::string fileName = "./tdm_";
#ifdef _MESH
    fileName += "mesh";
#else
    fileName += "torus";
#endif
    fileName += std::to_string(_NODES_ON_EDGE) + std::to_string(_NODES_ON_EDGE);
    fileName += "_" + std::to_string(_PACKET_SIZE) + "flit.csv";
    FILE *f = fopen(fileName.c_str(), "w");
    fprintf(f, "IR;Average;Maximum\n");
    int destination = 1 ;
    int IR = 1000 * _IR_START;
	double avg_hops = AverageHops(_NODES_ON_EDGE);
	int max_hops = MaximumHops(_NODES_ON_EDGE);
	
	// The waiting queue holds packets waiting to be sent off, while
	// the sink queue receives the packets and is used to calculate
	// the delay of the network
    std::queue<int> waiting_queue;
    std::queue<double> avg_queue;
	std::queue<int> max_queue;
    
	// The destination of the packets are generated from a uniform distribution
	// based on a pseudo-random number generator MT19937
    std::mt19937 engine(0); // Fixed seed of 0
    std::uniform_int_distribution<int> uniform_dist(0, _NODES);
    
	// Similarly, whether to inject a packet or not is determined by another
	// random number generator
    std::mt19937 engine2(0); // Fixed seed of 0
    std::uniform_int_distribution<int> injection(0, 1000);
    
    int tick = 0;
    int count = 0;
    double average = 0;
    int max = 0;
    
	// Generate results for all considered injection rates
    while (IR < _IR_END*1000)
	{
        // DRAIN QUEUE
        while (!waiting_queue.empty())
            waiting_queue.pop();
        
		// Run through n ticks - note that the maximum expected number of packages
		// in n ticks is 1/_TDM_PERIOD * IR * n, hence n should be large!
#ifdef _CONSTANT_NUM_PACKETS
        tick = 0;
        count = 0;
        while (count < _CONSTANT_NUM_PACKETS)
#else
        for (tick = 0; tick < 100000000; tick++) 
#endif
		{
            // SOURCE
			// If the random injection value is less than the current injection rate,
			// simulate generating a packet
            if (injection(engine2) < IR)
				// If additionally the random destination is the current destination
				// node, add the packet to the waiting queue
                if (uniform_dist(engine) == destination || trafficType)
					// _packets_ packets are inserted into the waiting queue in one tick
                    for (int i = 0; i < packets; i++)
                        waiting_queue.push(tick);
			
            // SINK
			// If a packet has been inserted (i.e. the waiting queue is non-empty),
			// check whether it can be sent off this cycle
            if (!waiting_queue.empty())
				// If the current tick matches the destination of the packet in the TDM
				// schedule, send the packet
                if ( tick % _TDM_PERIOD == 0 )
                {
					// calculate the delays of the packets
					avg_queue.push(((double) tick - waiting_queue.front()) * _TDM_SLOT_LENGTH + avg_hops * _LINK_DELAY + (avg_hops+1) * _ROUTER_DELAY);
					max_queue.push((tick - waiting_queue.front()) * _TDM_SLOT_LENGTH + max_hops * _LINK_DELAY + (max_hops+1) * _ROUTER_DELAY);
                    waiting_queue.pop();
#ifdef _CONSTANT_NUM_PACKETS
                    count++;
                }
            tick++;
#else
                }
#endif
        }
        
		// Empty the avg queue to find the average latency
		count = avg_queue.size();
        average = 0.0;
        while (!avg_queue.empty())
        {
            average += avg_queue.front();
            avg_queue.pop();
        }
        average /= count;
		
		// Empty the max queue to find the maximum experienced latency
		max = 0;
		while (!max_queue.empty())
		{
			max = MAX(max, max_queue.front());
			max_queue.pop();
		}
        
		// Print out the result depending on the requested output mode
        if (OutputMode == 1) 
		{
            if (latencyType) cout << average << endl;
            else cout << max << endl;
        } else if (OutputMode == 2)
            cout << "IR: " << (double)IR*0.001 << " Average Latency: " << average << endl;
		else
            cout << "IR: " << left << setw(5) << (double)IR*0.001 << " Average Latency: " << average << " Max Latency: " << max <<endl;
        fprintf(f, "%.3f;%.3f;%d\n", (double)IR*0.001, average, max);
        fflush(f);

		// Update the injection rate with the requested step
        IR += _IR_STEP*1000;
    }
    
	// Print out the considered injection rates if wanted
    double injection_rate = _IR_START;
    while (injection_rate < _IR_END && OutputMode == 1)
    {
       cout << injection_rate << endl;   
       injection_rate = injection_rate + _IR_STEP;
    }
    fclose(f);
}

// Calculates the average number of hops in a bi-torus network
double AverageHops(int N)
{
	// Include the sender on the hop count
    bool sent180 = false; 
    int hops = 0;
    int RLHops = 0;
    int RUHops = 0;
    int RHops = 0;
	// Total number of hops
    int sum = 0;
	// Number of nodes in the network
    int count = N*N - (!sent180 ? 1 : 0);
    
#ifdef _MESH
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            sum += i + j;
#else
	// Run through all the nodes and calculate the necessary number
	// of hops for reaching the node from node 0
    for (int i = 0; i < N; i++) 
	{
        for (int j = 0; j < N; j++) 
		{
            hops = i+j;
            RLHops = (N-i) + j; 
            RUHops = i + (N-j);
            RHops = (N-i) + (N-j);
            
            hops = MIN(hops,RLHops);
            RUHops = MIN(RUHops,RHops);
            hops = MIN(hops,RUHops);
            
            sum += hops;
        }
    }
#endif
    return (double)sum/count;
}

// Calculates the maximum number of hops in a bi-torus network
int MaximumHops(int N) 
{
	int hops = 0;
	int RLHops = 0;
	int RUHops = 0;
	int RHops = 0;
	int max = 0;
	
#ifdef _MESH
    max = (N-1) * 2;
#else
	// Run through all the nodes and calculate the necessary number
	// of hops for reaching the node from node 0
	for (int i = 0; i < N; i++) 
	{
		for (int j = 0; j < N; j++)
		{
			hops = i+j;
			RLHops = (N-i) + j; 
            RUHops = i + (N-j);
            RHops = (N-i) + (N-j);
			
			hops = MIN(hops,RLHops);
            RUHops = MIN(RUHops,RHops);
            hops = MIN(hops,RUHops);
			
			max = MAX(max, hops);
		}
	}
#endif
    return max;
}
