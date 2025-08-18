/*
    V1.7

    Genetic algorithm.
    - Generate a random population
    - Select parents weighted by their fitness
    - Mutate : Switch two random customers
    - Mutate : Move a customer from a ride to insert it in another ride (Can remove a ride)
    - Mutate : Create a ride with a random customer from another ride
*/

#include <cstdint>

#define CG_MODE       1
#define DEBUG_MODE    2
#define FINETUNE_MODE 3
#define CURRENT_MODE  CG_MODE
// #define CURRENT_MODE  DEBUG_MODE
// #define CURRENT_MODE FINETUNE_MODE

/* --- GENETIC ALGORITHM CONSTANTS --- */

int           N_ENTITIES = 10;
constexpr int N_GENERATION = INT32_MAX;
constexpr int N_ALLOWED_MILLISECONDS = 9000;

constexpr int ASSUMING_N_CUSTOMER_PER_RIDE = 40;
constexpr int ASSUMING_N_RIDE_PER_ENTITY = 40;

int MR_SWITCH_CUSTOMERS = 7;
int MR_MOVE_CUSTOMER = 16;
int MR_CREATE_RIDE = 2;

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
#include <sys/resource.h>
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

struct Ride
{
        int       customer_served;
        Location *customer_location[ASSUMING_N_CUSTOMER_PER_RIDE];
        int       capacity_left; // Start with constant vehicles capacity
};

int       get_ride_customer_served(Ride *ride) { return ride->customer_served; }
Location *get_ride_customer_location(Ride *ride, int index)
{
    return ride->customer_location[index];
}
int get_ride_capacity_left(Ride *ride) { return ride->capacity_left; }

void set_ride_customer_served(Ride *ride, int customer_served)
{
    ride->customer_served = customer_served;
}
void set_ride_customer_location(Ride *ride, int ride_location_index, Location *customer_loc)
{
    if (ride_location_index >= ASSUMING_N_CUSTOMER_PER_RIDE)
    {
        fprintf(
            stderr,
            "set_ride_customer_location(): ride_location_index %d >= ASSUMING_N_CUSTOMER_PER_RIDE "
            "%d\n",
            ride_location_index, ASSUMING_N_CUSTOMER_PER_RIDE
        );
    }
    ride->customer_location[ride_location_index] = customer_loc;
}
void set_ride_capacity_left(Ride *ride, int capacity_left) { ride->capacity_left = capacity_left; }

/* --- ENTITY --- */

struct Entity
{
        int  ride_count;
        Ride rides[ASSUMING_N_RIDE_PER_ENTITY];
};

int   get_entity_ride_count(Entity *entity) { return entity->ride_count; }
Ride *get_entity_ride(Entity *entity, int index)
{
    if (index >= ASSUMING_N_RIDE_PER_ENTITY)
    {
        fprintf(
            stderr, "get_entity_ride(): index %d >= ASSUMING_N_RIDE_PER_ENTITY %d\n", index,
            ASSUMING_N_RIDE_PER_ENTITY
        );
    }
    return &entity->rides[index];
}

void set_entity_ride_count(Entity *entity, int ride_count) { entity->ride_count = ride_count; }

/* --- STRUCTURE MANIPULATION - Ride --- */

void init_ride_with_customer(Ride *ride, Location *customer_loc)
{
    set_ride_customer_location(ride, 0, customer_loc);
    set_ride_customer_served(ride, 1);
    set_ride_capacity_left(ride, global_vehicle_capacity - get_location_demand(customer_loc));
}

/* --- STRUCTURE MANIPULATION - Entity --- */

int count_customer_locations(Entity *entity)
{
    int count = 0;

    for (int i = 0; i < get_entity_ride_count(entity); i++)
        count += get_ride_customer_served(get_entity_ride(entity, i));

    return count;
}

void remove_ride_from_entity(Entity *entity, int ride_index)
{
    // fprintf(
    //     stderr, "Removing ride at index %d/%d\n", ride_index, get_entity_ride_count(entity) - 1
    // );

    // Shift all rides after the removed one of one position
    for (int i = ride_index; i < get_entity_ride_count(entity) - 1; i++)
    {
        memcpy(get_entity_ride(entity, i), get_entity_ride(entity, i + 1), sizeof(Ride));
    }

    set_entity_ride_count(entity, get_entity_ride_count(entity) - 1);
}

void create_ride_to_entity(Entity *entity, Location *customer_loc)
{
    int actual_ride_count = get_entity_ride_count(entity);

    Ride *ride = get_entity_ride(entity, actual_ride_count);
    init_ride_with_customer(ride, customer_loc);

    set_entity_ride_count(entity, actual_ride_count + 1);
}

void add_customer_to_ride(Ride *ride, int customer_index, Location *customer_loc)
{
    // fprintf(
    //     stderr, "Adding customer %d to ride at index %d/%d\n", get_location_id(customer_loc),
    //     customer_index, get_ride_customer_served(ride) - 1
    // );

    // Shift all customers after the added one of one position
    for (int i = get_ride_customer_served(ride); i > customer_index; i--)
    {
        set_ride_customer_location(ride, i, get_ride_customer_location(ride, i - 1));
    }

    set_ride_customer_location(ride, customer_index, customer_loc);
    set_ride_customer_served(ride, get_ride_customer_served(ride) + 1);
    set_ride_capacity_left(ride, get_ride_capacity_left(ride) - get_location_demand(customer_loc));
}

void remove_customer_from_ride(Entity *entity, int ride_index, Ride *ride, int customer_index)
{
    // fprintf(
    //     stderr, "Removing customer %d from ride at index %d/%d\n",
    //     get_location_id(get_ride_customer_location(ride, customer_index)), customer_index,
    //     get_ride_customer_served(ride) - 1
    // );
    int new_customer_count = get_ride_customer_served(ride) - 1;

    if (new_customer_count == 0)
    {
        remove_ride_from_entity(entity, ride_index);
        return;
    }

    // Save the customer location to remove before it gets overwritten
    Location *customer_loc = get_ride_customer_location(ride, customer_index);

    // Shift all customers after the removed one of one position
    for (int i = customer_index; i < new_customer_count; i++)
    {
        set_ride_customer_location(ride, i, get_ride_customer_location(ride, i + 1));
    }

    set_ride_customer_served(ride, new_customer_count);
    set_ride_capacity_left(ride, get_ride_capacity_left(ride) + get_location_demand(customer_loc));
}

/* --- STRUCTURE RELATED --- */

bool can_customer_be_added_to_ride(Ride *ride, Location *customer_loc)
{
    return get_ride_capacity_left(ride) >= get_location_demand(customer_loc);
}

string create_entity_string(Entity *entity)
{
    string str = "";

    for (int i = 0; i < get_entity_ride_count(entity); i++)
    {
        Ride *ride = get_entity_ride(entity, i);
        for (int j = 0; j < get_ride_customer_served(ride); j++)
        {
            str += " " + to_string(get_location_id(get_ride_customer_location(ride, j)));
        }
        str += ";";
    }

    return str;
}

void print_entity(Entity *entity)
{
    fprintf(stderr, "\nEntity: %d rides\n", get_entity_ride_count(entity));

    for (int r = 0; r < get_entity_ride_count(entity); r++)
    {
        Ride *ride = get_entity_ride(entity, r);
        fprintf(
            stderr, "Entity - Ride %d: %d customers (%d capacity left)\n", r,
            get_ride_customer_served(ride), get_ride_capacity_left(ride)
        );

        for (int c = 0; c < get_ride_customer_served(ride); c++)
        {
            Location *customer = get_ride_customer_location(ride, c);
            fprintf(
                stderr, "Entity - Ride %d - Customer %d: Demands %d\n", r,
                get_location_id(customer), get_location_demand(customer)
            );
        }
    }
}

/* --- GENETIC ALGORITHM - INITIALISATION --- */

void init_entity(Entity *entity)
{
    int customer_ids[global_customer_count];
    memcpy(customer_ids, global_customer_ids, sizeof(customer_ids));
    shuffle(customer_ids, customer_ids + global_customer_count, rand_engine);

    // fprintf(stderr, "\ninit_entity: Customer count = %d\n", global_customer_count);

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

        Ride *ride = get_entity_ride(entity, ride_index);

        // Verify current ride demand and customer one don't exceed vehicle capacity
        if (ride_demand + cust_demand > global_vehicle_capacity)
        {
            // Set current ride final demand before going next
            set_ride_capacity_left(ride, global_vehicle_capacity - ride_demand);
            set_ride_customer_served(ride, ride_customer_count);
            // fprintf(
            //     stderr, "init_entity: Ride %d has %d customers (%d demand left)\n", ride_index,
            //     get_ride_customer_served(ride), get_ride_capacity_left(ride)
            // );

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
        // fprintf(
        //     stderr, "init_entity: Adding customer %d to ride %d (demand = %d/%d, address =
        //     %p)\n", cust_id, ride_index, ride_demand, global_vehicle_capacity,
        //     get_ride_customer_location(ride, ride_customer_count - 1)
        // );
    }

    Ride *ride = get_entity_ride(entity, ride_index);
    set_ride_capacity_left(ride, global_vehicle_capacity - ride_demand);
    set_ride_customer_served(ride, ride_customer_count);
    // fprintf(
    //     stderr, "init_entity: Ride %d has %d customers (%d demand left)\n", ride_index,
    //     get_ride_customer_served(ride), get_ride_capacity_left(ride)
    // );

    set_entity_ride_count(entity, ride_index + 1);
    // fprintf(stderr, "init_entity: Entity has %d rides\n", get_entity_ride_count(entity));

    if (count_customer_locations(entity) != global_customer_count)
    {
        fprintf(
            stderr, "init_entity(): count_customer_locations() = %d / %d\n",
            count_customer_locations(entity), global_customer_count
        );
        exit(0);
    }
}

void init_population(Entity *population)
{
    memset(population, 0, sizeof(Entity) * N_ENTITIES);
    for (int i = 0; i < N_ENTITIES; i++)
    {
        init_entity(&population[i]);
        // print_entity(&population[i]);
    }
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

int compute_ride_fitness(Ride *ride)
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

int compute_fitness(Entity *entity)
{
    int fitness = 0;

    for (int i = 0; i < get_entity_ride_count(entity); i++)
        fitness += compute_ride_fitness(get_entity_ride(entity, i));

    return fitness;
}

/* --- GENETIC ALGORITHM - SELECTION --- */

Entity *get_best_entity(Entity *population)
{
    int max_fitness = INT_MAX;
    int best_entity_index = 0;
    for (int i = 0; i < N_ENTITIES; i++)
    {
        int fitness = compute_fitness(&population[i]);

        if (fitness < max_fitness)
        {
            max_fitness = fitness;
            best_entity_index = i;
        }
    }

    return &population[best_entity_index];
}

void select_next_generation_entities(Entity *population)
{
    int fitnesses[N_ENTITIES];
    int max_fitness = 0;
    for (int i = 0; i < N_ENTITIES; i++)
    {
        fitnesses[i] = compute_fitness(&population[i]);
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

void switch_customers(Entity *entity)
{
    int rnd_ride_i1 = rand() % get_entity_ride_count(entity);
    int rnd_ride_i2 = rand() % get_entity_ride_count(entity);

    Ride *ride1 = get_entity_ride(entity, rnd_ride_i1);
    Ride *ride2 = get_entity_ride(entity, rnd_ride_i2);

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

    if (count_customer_locations(entity) != global_customer_count)
    {
        fprintf(
            stderr,
            "switch_customers(): count_customer_locations() %d != "
            "global_customer_count %d\n",
            count_customer_locations(entity), global_customer_count
        );
        exit(0);
    }
}

void move_customer(Entity *entity)
{
    // Choose 2 random rides
    int   rnd_ride_i_dst = rand() % get_entity_ride_count(entity);
    int   rnd_ride_i_src = rand() % get_entity_ride_count(entity);
    Ride *ride_dst = get_entity_ride(entity, rnd_ride_i_dst);
    Ride *ride_src = get_entity_ride(entity, rnd_ride_i_src);

    // Choose a random customer in the source ride
    int       rnd_customer_i_src = rand() % get_ride_customer_served(ride_src);
    Location *customer_to_move = get_ride_customer_location(ride_src, rnd_customer_i_src);

    // Verify it can be added to the destination ride
    if (rnd_ride_i_dst == rnd_ride_i_src ||
        can_customer_be_added_to_ride(ride_dst, customer_to_move))
    {
        // We cannot remove a ride if it's the one selected as destination
        if (rnd_ride_i_dst == rnd_ride_i_src && get_ride_customer_served(ride_src) - 1 == 0)
            return;

        // Choose a random position to insert it in the destination ride
        int rnd_customer_i_dst = rand() % get_ride_customer_served(ride_dst);
        add_customer_to_ride(ride_dst, rnd_customer_i_dst, customer_to_move);

        // If the rides are the same and the dst index is before the src index, that means the src
        // customer has been shifted of 1 place due to the dst insertion.
        if (rnd_ride_i_dst == rnd_ride_i_src && rnd_customer_i_dst < rnd_customer_i_src)
            rnd_customer_i_src++;

        remove_customer_from_ride(entity, rnd_ride_i_src, ride_src, rnd_customer_i_src);
    }

    if (count_customer_locations(entity) != global_customer_count)
    {
        fprintf(
            stderr,
            "move_customer(): count_customer_locations() %d != "
            "global_customer_count %d\n",
            count_customer_locations(entity), global_customer_count
        );
        exit(0);
    }
}

void create_ride_with_random_customer(Entity *entity)
{
    // Choose a random ride to remove a customer from
    int   rnd_ride_i_src = rand() % get_entity_ride_count(entity);
    Ride *ride_src = get_entity_ride(entity, rnd_ride_i_src);

    // Don't move the customer if it's the only one in its ride (Prevent useless actions)
    if (get_ride_customer_served(ride_src) == 1)
    {
        return;
    }

    // Choose a random customer to move
    int       rnd_customer_i_src = rand() % get_ride_customer_served(ride_src);
    Location *customer_to_move = get_ride_customer_location(ride_src, rnd_customer_i_src);

    remove_customer_from_ride(entity, rnd_ride_i_src, ride_src, rnd_customer_i_src);
    create_ride_to_entity(entity, customer_to_move);

    if (count_customer_locations(entity) != global_customer_count)
    {
        fprintf(
            stderr,
            "create_ride_with_random_customer(): count_customer_locations() %d != "
            "global_customer_count %d\n",
            count_customer_locations(entity), global_customer_count
        );
        exit(0);
    }
}

void mutate_entity(Entity *entity)
{
    int rnd_number = rand() % 100;
    if (rnd_number < MR_SWITCH_CUSTOMERS)
        switch_customers(entity);

    rnd_number = rand() % 100;
    if (rnd_number < MR_MOVE_CUSTOMER)
        move_customer(entity);

    rnd_number = rand() % 100;
    if (rnd_number < MR_CREATE_RIDE)
        create_ride_with_random_customer(entity);
}

void mutate_population(Entity *population)
{
    for (int i = 0; i < N_ENTITIES; i++)
        mutate_entity(&population[i]);
}

/* --- MAIN FUNCTIONS --- */

void parse_stdin()
{
    if (CURRENT_MODE == FINETUNE_MODE)
    {
        int seed;
        cin >> N_ENTITIES >> MR_SWITCH_CUSTOMERS >> MR_MOVE_CUSTOMER >> MR_CREATE_RIDE >> seed;
        cin.ignore();

        std::srand(seed);
    }

    cin >> global_location_count;
    cin.ignore();

    cin >> global_vehicle_capacity;
    cin.ignore();

    // cerr << global_location_count << " " << global_vehicle_capacity << endl;
    // fprintf(
    //     stderr, "parse_stdin: Location count: %d | Vehicle capacity: %d\n",
    //     global_location_count, global_vehicle_capacity
    // );

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
}

int main()
{
    parse_stdin();

    struct rlimit rl;
    getrlimit(RLIMIT_STACK, &rl);
    rl.rlim_cur = rl.rlim_max;
    setrlimit(RLIMIT_STACK, &rl);

    auto start = chrono::high_resolution_clock::now();

    Entity population[N_ENTITIES];
    init_population(population);

    Entity *best_entity = get_best_entity(population);
    int     best_first_fitness = compute_fitness(best_entity);
    int     best_fitness = best_first_fitness;

    fprintf(
        stderr,
        "Starting GA with %d entities | Mutation rates: Switch c=%d%%, Move c=%d%%, Create "
        "r=%d%%\n",
        N_ENTITIES, MR_SWITCH_CUSTOMERS, MR_MOVE_CUSTOMER, MR_CREATE_RIDE
    );

    int  generation_count = 0;
    auto end = chrono::high_resolution_clock::now();
    while (chrono::duration_cast<chrono::milliseconds>(end - start).count() <
               N_ALLOWED_MILLISECONDS &&
           generation_count < N_GENERATION)
    {
        select_next_generation_entities(population);
        mutate_population(population);
        generation_count++;

        Entity *entity = get_best_entity(population);
        int     fitness = compute_fitness(entity);
        if (fitness < best_fitness)
        {
            best_fitness = fitness;
            best_entity = entity;
        }

        end = chrono::high_resolution_clock::now();
        // fprintf(
        //     stderr, "Best fitness after %ldms and %d generations (of %d entities): %d -> %d\n",
        //     chrono::duration_cast<chrono::milliseconds>(end - start).count(), generation_count,
        //     N_ENTITIES, best_first_fitness, best_fitness
        // );
    }

    fprintf(
        stderr, "Best fitnesses after %d generations (of %d entities): %d -> %d\n",
        generation_count, N_ENTITIES, best_first_fitness, best_fitness
    );

    if (CURRENT_MODE == CG_MODE)
        cout << create_entity_string(best_entity) << endl;
    else if (CURRENT_MODE == DEBUG_MODE)
        cout << "ent=" << N_ENTITIES << " | gen=" << generation_count
             << " | fitness=" << best_fitness << endl;
    else if (CURRENT_MODE == FINETUNE_MODE)
        cout << best_fitness << endl;
}
