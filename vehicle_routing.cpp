
// Iterate until time is over :
// Generate random population, keep the best score/response

#undef _GLIBCXX_DEBUG
#pragma GCC optimize("Ofast,unroll-loops,omit-frame-pointer,inline")
#pragma GCC option("arch=native", "tune=native", "no-zero-upper")
#pragma GCC target(                                                                                \
    "movbe,aes,pclmul,avx,avx2,f16c,fma,sse3,ssse3,sse4.1,sse4.2,rdrnd,popcnt,bmi,bmi2,lzcnt"      \
)

#include <algorithm> // for std::shuffle
#include <chrono>
#include <climits>
#include <cstring>
#include <iostream>
#include <random> // for std::mt19937 and std::random_device
#include <string>
#include <vector>

using namespace std;

// Setup random engine
std::random_device rand_device;
std::mt19937       rand_engine(rand_device());

/* --- CUSTOMER --- */

struct Location
{
        int id;     // The id of the Location (0 is the depot)
        int x;      // The x coordinate of the Location
        int y;      // The y coordinate of the Location
        int demand; // The demand
};

constexpr int MAX_CUSTOMERS = 199;
int           global_customer_count;
int           global_customer_ids[MAX_CUSTOMERS]; // Customers and locations are the same
int           global_location_count;
Location      global_locations[MAX_CUSTOMERS + 1];      // 0 is empty
Location *global_depot_location = &global_locations[0]; // Address to global_locations first element

int get_location_id(Location *location) { return location->id; }
int get_location_x(Location *location) { return location->x; }
int get_location_y(Location *location) { return location->y; }
int get_location_demand(Location *location) { return location->demand; }

void set_location_id(Location *location, int id) { location->id = id; }
void set_location_x(Location *location, int x) { location->x = x; }
void set_location_y(Location *location, int y) { location->y = y; }
void set_location_demand(Location *location, int demand) { location->demand = demand; }

void print_location(Location *location)
{
    cerr << "Location " << location->id << " at (" << location->x << ", " << location->y
         << ")\t demands " << location->demand << endl;
}

/* --- RIDE --- */

int global_vehicle_capacity;

// TODO: Don't use MAX_CUSTOMERS
struct Ride
{
        int       customer_served;
        Location *customer_location[MAX_CUSTOMERS];
        int       capacity_left; // Start with constant vehicles capacity
};

int       get_ride_customer_served(Ride &ride) { return ride.customer_served; }
Location *get_ride_customer_location(Ride &ride, int index)
{
    return ride.customer_location[index];
}
int get_ride_capacity_left(Ride &ride) { return ride.capacity_left; }

void set_ride_customer_served(Ride &ride, int customer_served)
{
    ride.customer_served = customer_served;
}
void set_ride_customer_location(Ride &ride, int ride_location_index, Location *customer_loc)
{
    ride.customer_location[ride_location_index] = customer_loc;
}
void set_ride_capacity_left(Ride &ride, int capacity_left) { ride.capacity_left = capacity_left; }

/* --- ENTITY --- */

// TODO: Don't use MAX_CUSTOMERS
struct Entity
{
        int  ride_count;
        Ride rides[MAX_CUSTOMERS];
};

int   get_entity_ride_count(Entity &entity) { return entity.ride_count; }
Ride &get_entity_ride(Entity &entity, int index) { return entity.rides[index]; }

void set_entity_ride_count(Entity &entity, int ride_count) { entity.ride_count = ride_count; }

string create_entity_string(Entity &entity)
{
    string str = "";

    for (int i = 0; i < get_entity_ride_count(entity); i++)
    {
        Ride &ride = get_entity_ride(entity, i);
        for (int j = 0; j < get_ride_customer_served(ride); j++)
        {
            str += " " + to_string(get_location_id(get_ride_customer_location(ride, j)));
        }
        str += ";";
    }

    return str;
}

/* --- POPULATION --- */

constexpr int N_ENTITIES = 100;

Entity population[N_ENTITIES];

/* --- GENETIC ALGORITHM - INITIALISATION --- */

void init_ride(Ride &ride, int *customer_ids, int customer_index, int customer_count)
{
    set_ride_customer_served(ride, customer_count);
    for (int i = 0, c_i = customer_index; c_i < customer_index + customer_count; i++, c_i++)
        set_ride_customer_location(ride, i, &global_locations[customer_ids[c_i]]);
    set_ride_capacity_left(ride, global_vehicle_capacity);
}

void init_entity(Entity &entity)
{
    int customer_ids[global_customer_count];
    memcpy(customer_ids, global_customer_ids, sizeof(customer_ids));
    shuffle(customer_ids, customer_ids + global_customer_count, rand_engine);

    // fprintf(stderr, "init_entity: Customer count = %d\n", global_customer_count);

    int customer_index = 0;
    int ride_index = 0;
    int ride_first_customer_index = customer_index;
    int ride_customer_count = 0;
    int ride_demand = 0;
    while (customer_index < global_customer_count)
    {
        // Add customers to this ride until capacity would be exceeded
        int cust_id = customer_ids[customer_index];
        int cust_demand = global_locations[cust_id].demand;

        ride_demand += cust_demand;
        if (ride_demand > global_vehicle_capacity)
        {
            // Exceed capacity, start a new ride
            // fprintf(stderr, "init_ride %d at customer_index %d: customer_count=%d\n", ride_index,
            // ride_first_customer_index, ride_customer_count);
            init_ride(
                get_entity_ride(entity, ride_index), customer_ids, ride_first_customer_index,
                ride_customer_count
            );
            ride_index++;
            ride_first_customer_index = customer_index;
            ride_customer_count = 0;
            ride_demand = cust_demand;
        }

        customer_index++;
        ride_customer_count++;
    }
    // fprintf(stderr, "init_ride %d at customer_index %d: customer_count=%d\n", ride_index,
    // ride_first_customer_index, ride_customer_count);
    init_ride(
        get_entity_ride(entity, ride_index), customer_ids, ride_first_customer_index,
        ride_customer_count
    );

    set_entity_ride_count(entity, ride_index + 1);
}

void init_population()
{
    // // Sum of demands of all customers
    // int total_demand = 0;
    // for (int i = 1; i < global_location_count; i++)
    //     total_demand += get_location_demand(&global_locations[i]);

    // int ride_count_min = (total_demand / global_vehicle_capacity) +
    //                      (total_demand % global_vehicle_capacity == 0 ? 0 : 1);
    // int ride_count_max = global_customer_count;

    // cerr << "total_demand of " << total_demand << " sits." << endl;
    // cerr << "ride_count_min: " << ride_count_min << " " << "ride_count_max: " << ride_count_max
    //      << endl;

    memset(population, 0, sizeof(population));
    for (int i = 0; i < N_ENTITIES; i++)
        init_entity(population[i]);
}

/* --- GENETIC ALGORITHM - EVALUATION --- */

int euclidienne_distance(int x1, int y1, int x2, int y2)
{
    // TODO: Approximate sqrt with a faster function ?
    int dx = x1 - x2;
    int dy = y1 - y2;
    return round(sqrt(dx * dx + dy * dy));
}

int euclidienne_distance(Location *loc1, Location *loc2)
{
    return euclidienne_distance(
        get_location_x(loc1), get_location_y(loc1), get_location_x(loc2), get_location_y(loc2)
    );
}

int compute_ride_fitness(Ride &ride)
{
    int fitness = 0;

    Location *loc1 = global_depot_location;
    Location *loc2;
    for (int i = 0; i < get_ride_customer_served(ride); i++)
    {
        loc2 = get_ride_customer_location(ride, i);
        fitness += euclidienne_distance(loc1, loc2);

        loc1 = loc2;
    }

    loc2 = global_depot_location;
    fitness += euclidienne_distance(loc1, loc2);

    return fitness;
}

int compute_fitness(Entity &entity)
{
    int fitness = 0;

    for (int i = 0; i < get_entity_ride_count(entity); i++)
        fitness += compute_ride_fitness(get_entity_ride(entity, i));

    return fitness;
}

/* --- GENETIC ALGORITHM - SELECTION --- */

Entity &get_best_entity()
{
    int max_fitness = INT_MAX;
    int best_entity_index = 0;
    for (int i = 0; i < N_ENTITIES; i++)
    {
        int fitness = compute_fitness(population[i]);

        if (fitness < max_fitness)
        {
            max_fitness = fitness;
            best_entity_index = i;
        }
    }

    return population[best_entity_index];
}

void select_next_generation_entities()
{
    int fitnesses[N_ENTITIES];
    int max_fitness = 0;
    for (int i = 0; i < N_ENTITIES; i++)
    {
        fitnesses[i] = compute_fitness(population[i]);
        max_fitness = max(max_fitness, fitnesses[i]);
    }

    // Give a chance to the worst entities to be selected
    max_fitness++;
 
    // Sum all fitnesses iteratively and save it for all entities
    int fitnesses_sum_checkpoints[N_ENTITIES];
    int fitnesses_sum = 0;
    for (int i = 0; i < N_ENTITIES; i++)
    {
        // Revert fitnesses to make the lower fitnesses more likely to be selected
        // The lower the fitness is, the higher the probability of being selected
        fitnesses_sum += max_fitness - fitnesses[i];
        fitnesses_sum_checkpoints[i] = fitnesses_sum;
    }

    Entity new_population[N_ENTITIES];
    for (int i = 0; i < N_ENTITIES; i++)
    {
        // Select randomly N_ENTITIES entities from the population weighted by their fitness
        int rnd_number = rand() % fitnesses_sum;

        // Higher checkpoint gap are more likely to contain the random number
        int index = 0;
        while (rnd_number > fitnesses_sum_checkpoints[index])
            index++;

        // Keep the selected entity in the new population
        memcpy(&new_population[i], &population[index], sizeof(Entity));
    }

    memcpy(population, new_population, sizeof(Entity) * N_ENTITIES);
}

/* --- GENETIC ALGORITHM - CROSSOVER --- */

/* --- GENETIC ALGORITHM - MUTATION --- */

/* --- MAIN FUNCTIONS --- */

void parse_stdin()
{
    cin >> global_location_count;
    cin.ignore();
    cerr << "parse_stdin: Location count: " << global_location_count << endl;

    cin >> global_vehicle_capacity;
    cin.ignore();
    cerr << "parse_stdin: Vehicle capacity: " << global_vehicle_capacity << endl;

    for (int i = 0; i < global_location_count; i++)
    {

        int id;     // The id of the location (0 is the depot)
        int x;      // The x coordinate of the location
        int y;      // The y coordinate of the location
        int demand; // The demand
        cin >> id >> x >> y >> demand;
        // cerr << "parse_stdin: Location " << id << ": " << x << " " << y << " demand " << demand
            //  << endl;

        Location *location = &global_locations[i];
        set_location_id(location, id);
        set_location_x(location, x);
        set_location_y(location, y);
        set_location_demand(location, demand);

        if (id != 0)
            global_customer_ids[id - 1] = id;

        cin.ignore();
    }

    // Depot is the first location in the list
    global_customer_count = global_location_count - 1;
}

int main()
{
    parse_stdin();

    // for (int i = 0; i < global_customer_count; i++)
    //     cerr << "Demand in location id " << i << ": " << global_customer_ids[i]
    //          << endl; // 0 is the depot, so we skip the first custom

    // cerr << "Parsed " << global_location_count << " locations" << endl;
    // for (int i = 1; i < global_location_count; i++)
    //     print_location(&global_locations[i]);

    // cerr << "----------------------------------------" << endl;
    // for (int i = 0; i < N_ENTITIES; i++)
    //     cerr << "Entity " << i << " has " << get_entity_ride_count(population[i])
    //          << " rides (fitness=" << compute_fitness(population[i])
    //          << "): " << create_entity_string(population[i]) << endl;

    auto start = chrono::high_resolution_clock::now();

    init_population();
    Entity &best_entity = get_best_entity();
    string best_string = create_entity_string(best_entity);
    int best_fitness = compute_fitness(best_entity);
    cerr << "Best entity fitness: " << best_fitness << endl;

    auto end = chrono::high_resolution_clock::now();
    while (chrono::duration_cast<chrono::milliseconds>(end - start).count() < 9500)
    {
        init_population();
        best_entity = get_best_entity();
        string entity_string = create_entity_string(best_entity);
        int fitness = compute_fitness(best_entity);
        if (fitness < best_fitness)
        {
            best_string = entity_string;
            best_fitness = fitness;
        }
        
        end = chrono::high_resolution_clock::now();
    }
    
    cerr << "Best entity fitness: " << best_fitness << endl;
    cout << best_string << endl;
}
