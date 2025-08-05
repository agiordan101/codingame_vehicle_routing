
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

/* --- GENETIC ALGORITHM --- */

constexpr int N_ENTITIES = 1;
constexpr int MR_CUSTOMER_SWITCH = 40;

/* --- GENETIC ALGORITHM - INITIALISATION --- */

void init_entity(Entity &entity)
{
    int customer_ids[global_customer_count];
    memcpy(customer_ids, global_customer_ids, sizeof(customer_ids));
    shuffle(customer_ids, customer_ids + global_customer_count, rand_engine);

    fprintf(stderr, "\ninit_entity: Customer count = %d\n", global_customer_count);

    int customer_ids_index = 0;
    int ride_index = 0;
    int ride_customer_count = 0;
    int ride_demand = 0;

    // Start the first ride
    while (customer_ids_index < global_customer_count)
    {
        int       cust_id = customer_ids[customer_ids_index++];
        Location *cust = &global_locations[cust_id];
        int       cust_demand = get_location_demand(cust);

        Ride &ride = get_entity_ride(entity, ride_index);

        // Verify current ride demand and customer one don't exceed vehicle capacity
        if (ride_demand + cust_demand > global_vehicle_capacity)
        {
            // Set current ride final demand before going next
            set_ride_capacity_left(ride, global_vehicle_capacity - ride_demand);
            set_ride_customer_served(ride, ride_customer_count);
            // fprintf(
            //     stderr, "init_entity: Ride %d has %d customers (%d demand left)\n", ride_index,
            //     ride_customer_count, global_vehicle_capacity - ride_demand
            // );
            fprintf(
                stderr, "init_entity: Ride %d has %d customers (%d demand left)\n", ride_index,
                get_ride_customer_served(ride), get_ride_capacity_left(ride)
            );

            // And start a new ride
            ride_index++;
            ride_customer_count = 0;
            ride_demand = cust_demand;
            ride = get_entity_ride(entity, ride_index);
        }
        else
        {
            // Sum customer demand to the current ride demand
            ride_demand += cust_demand;
        }

        // Add customer to the current ride
        set_ride_customer_location(ride, ride_customer_count++, cust);

        fprintf(
            stderr, "init_entity: Adding customer %d to ride %d (demand = %d/%d, address = %p)\n",
            cust_id, ride_index, ride_demand, global_vehicle_capacity,
            get_ride_customer_location(ride, ride_customer_count - 1)
        );

        ride = get_entity_ride(entity, ride_index);

        fprintf(
            stderr, "init_entity: Adding customer %d to ride %d (demand = %d/%d, address = %p)\n",
            cust_id, ride_index, ride_demand, global_vehicle_capacity,
            get_ride_customer_location(ride, ride_customer_count - 1)
        );
    }

    Ride &ride = get_entity_ride(entity, ride_index);
    set_ride_capacity_left(ride, global_vehicle_capacity - ride_demand);
    set_ride_customer_served(ride, ride_customer_count);

    fprintf(
        stderr, "init_entity: Ride %d has %d customers (%d demand left)\n", ride_index,
        get_ride_customer_served(ride), get_ride_capacity_left(ride)
    );

    set_entity_ride_count(entity, ride_index + 1);
    // fprintf(stderr, "init_entity: Entity has %d rides\n", ride_index + 1);
    fprintf(stderr, "init_entity: Entity has %d rides\n", get_entity_ride_count(entity));

    Ride *ride_ptr = &entity.rides[ride_index];
    fprintf(
        stderr, "Entity N - Ride %d: %d customers (%d capacity left) | Location address %p\n",
        ride_index, ride_ptr->customer_served, ride_ptr->capacity_left,
        ride_ptr->customer_location[ride_index]
    );
}

void init_population(Entity *population)
{
    memset(population, 0, sizeof(Entity) * N_ENTITIES);
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

Entity &get_best_entity(Entity *population)
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

void select_next_generation_entities(Entity *population)
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

void switch_customers(Entity &entity)
{
    int rnd_ride_i1 = rand() % get_entity_ride_count(entity);
    int rnd_ride_i2 = rand() % get_entity_ride_count(entity);

    Ride &ride1 = get_entity_ride(entity, rnd_ride_i1);
    Ride &ride2 = get_entity_ride(entity, rnd_ride_i2);

    int rnd_customer_i1 = rand() % get_ride_customer_served(ride1);
    int rnd_customer_i2 = rand() % get_ride_customer_served(ride2);

    Location *customer1 = get_ride_customer_location(ride1, rnd_customer_i1);
    Location *customer2 = get_ride_customer_location(ride2, rnd_customer_i2);

    // Moving customer within the same ride doesn't require capacity checks
    if (rnd_ride_i1 != rnd_ride_i2)
    {
        // Verify rides can accept the other customer instead
        int new_ride1_capacity = get_ride_capacity_left(ride1) + get_location_demand(customer1) -
                                 get_location_demand(customer2);
        if (new_ride1_capacity < 0)
            return;

        // Verify rides can accept the other customer instead
        int new_ride2_capacity = get_ride_capacity_left(ride2) + get_location_demand(customer2) -
                                 get_location_demand(customer1);
        if (new_ride2_capacity < 0)
            return;

        set_ride_capacity_left(ride1, new_ride1_capacity);
        set_ride_capacity_left(ride2, new_ride2_capacity);
    }

    set_ride_customer_location(ride1, rnd_customer_i1, customer2);
    set_ride_customer_location(ride2, rnd_customer_i2, customer1);
}

void mutate_entity(Entity &entity)
{
    int rnd_number = rand() % 100;

    if (rnd_number < MR_CUSTOMER_SWITCH)
        switch_customers(entity);
}

void mutate_population(Entity *population)
{
    for (int i = 0; i < N_ENTITIES; i++)
        mutate_entity(population[i]);
}

/* --- MAIN FUNCTIONS --- */

void parse_stdin()
{
    cin >> global_location_count;
    cin.ignore();

    cin >> global_vehicle_capacity;
    cin.ignore();

    // cerr << global_location_count << " " << global_vehicle_capacity << endl;
    fprintf(
        stderr, "parse_stdin: Location count: %d | Vehicle capacity: %d\n", global_location_count,
        global_vehicle_capacity
    );

    for (int i = 0; i < global_location_count; i++)
    {

        int id;     // The id of the location (0 is the depot)
        int x;      // The x coordinate of the location
        int y;      // The y coordinate of the location
        int demand; // The demand
        cin >> id >> x >> y >> demand;
        // cerr << id << " " << x << " " << y << " " << demand << endl;

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

    Entity population[N_ENTITIES];

    init_population(population);
    int generation_count = 1;

    for (int i = 0; i < N_ENTITIES; i++)
    {
        Entity &entity = population[i];
        Entity *entity_ptr = &population[i];
        fprintf(stderr, "Entity %d: %d rides\n", i, get_entity_ride_count(entity));
        // fprintf(stderr, "Entity %d: '%s'\n", i, create_entity_string(entity).c_str());

        for (int r = 0; r < get_entity_ride_count(entity); r++)
        {
            Ride &ride = get_entity_ride(entity, r);
            fprintf(
                stderr, "Entity %d - Ride %d: %d customers (%d capacity left) | First at %d %d\n",
                i, r, get_ride_customer_served(ride), get_ride_capacity_left(ride),
                get_location_x(get_ride_customer_location(ride, 0)),
                get_location_y(get_ride_customer_location(ride, 0))
            );

            Ride *ride_ptr = &entity_ptr->rides[r];
            fprintf(
                stderr, "Entity %d - Ride %d: %d customers (%d capacity left) | First at %d %d\n",
                i, r, ride_ptr->customer_served, ride_ptr->capacity_left,
                ride_ptr->customer_location[0]->x, ride_ptr->customer_location[0]->y
            );

            for (int c = 0; c < get_ride_customer_served(ride); c++)
            {
                Location *customer = get_ride_customer_location(ride, c);
                fprintf(
                    stderr, "Entity %d - Ride %d - Customer %d: Demands %d\n", i, r, c,
                    get_location_demand(customer)
                );
            }
        }
    }

    Entity &best_entity = get_best_entity(population);
    fprintf(
        stderr, "Generation %d: %s (fitness=%d)\n", generation_count,
        create_entity_string(best_entity).c_str(), compute_fitness(best_entity)
    );

    int    best_first_fitness = compute_fitness(get_best_entity(population));
    int    best_fitness = best_first_fitness;
    string best_string = "";

    auto end = chrono::high_resolution_clock::now();
    while (chrono::duration_cast<chrono::milliseconds>(end - start).count() < 9000)
    {
        // init_population();
        select_next_generation_entities(population);
        mutate_population(population);
        generation_count++;

        Entity &best_entity = get_best_entity(population);
        string  entity_string = create_entity_string(best_entity);
        int     fitness = compute_fitness(best_entity);
        if (fitness < best_fitness)
        {
            best_fitness = fitness;
            best_string = entity_string;
        }

        fprintf(
            stderr, "Best fitnesses after %d generations (of %d entities): %d -> %d\n",
            generation_count, N_ENTITIES, best_first_fitness, best_fitness
        );
        end = chrono::high_resolution_clock::now();
    }

    fprintf(
        stderr, "Best fitnesses after %d generations (of %d entities): %d -> %d\n",
        generation_count, N_ENTITIES, best_first_fitness, best_fitness
    );
    cout << best_string << endl;
}
