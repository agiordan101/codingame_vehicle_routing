import random
import subprocess
import os
import numpy as np

# ---- Config ----

GA_GEN_LIMIT = 10
POPULATION_SIZE = 10       # number of individuals per generation
MUTATION_RATE = 0.33     # probability of mutation per genome element
N_SEEDS = 1

# Ranges for random initialization
RANGES = {
    "entities": (2, 30),
    "mr_switch": (1, 50),
    "mr_move": (1, 50),
    "mr_create": (1, 10)
}

CPP_EXEC = "./bins/vehicle_routing"
ENTITIES_FILE = "./finetuning_with_ga/entities_file.csv"
BEST_ENTITIES_FILE = "./finetuning_with_ga/best_entities_file.csv"
TMP_FILE = "./finetuning_with_ga/temp_input.txt"


def get_test_files_from_folder(folder_path: str) -> list[str]:
    """Récupère la liste des fichiers .txt dans un dossier."""
    test_files = []
    for file in os.listdir(folder_path):
        test_files.append(os.path.join(folder_path, file))
    return test_files


def random_genome():
    """Create a random genome within the given ranges."""
    return [
        random.randint(*RANGES["entities"]),
        random.randint(*RANGES["mr_switch"]),
        random.randint(*RANGES["mr_move"]),
        random.randint(*RANGES["mr_create"])
    ]


def init_random_population():
    return [random_genome() for _ in range(POPULATION_SIZE)]


def init_population():
    return [
        [10, 6, 12, 3],
        [9, 6, 8, 4],
        [14, 7, 12, 4],
        [30, 6, 11, 6],
        [9, 6, 12, 4],
        [8, 6, 13, 4],
        [8, 6, 12, 4],
        [12, 6, 22, 4],
        [17, 6, 22, 4],
        [9, 6, 31, 4],
    ]


def run_test_with_config(config_integers: tuple, seed: int, file_name: str):
    """Run the test with GA constants + seed and return the result."""
    config_integers = list(config_integers) + [seed]
    print(config_integers)

    # Read test file content
    with open(file_name, 'r') as f:
        content = f.read()

    # Prefix GA constants
    prefixed_content = ' '.join(map(str, config_integers)) + '\n' + content

    # Write temp input file
    with open(TMP_FILE, 'w') as f:
        f.write(prefixed_content)

    # Appeler l'exécutable C++ avec le fichier temporaire comme entrée
    result = subprocess.run(
        [CPP_EXEC],
        stdin=open(TMP_FILE, 'r'),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )

    # Supprimer le fichier temporaire
    os.remove(TMP_FILE)

    # Récupérer et convertir le résultat
    try:
        output = result.stdout.strip()

        if len(output) == 0:
            print(f"Erreur de conversion pour {file_name}. RETRY.")
            output = run_test_with_config(config_integers, seed, file_name)

        output = int(output)

    except Exception as e:
        print(f"Erreur de conversion pour {file_name}:")
        print(e)
        raise e

    return output

def process_test_files(entity: list[int], seed: int, test_files: list[str]):
    """Run all test files for a given genome+seed and return the sum of results."""
    return sum(
        run_test_with_config(entity, seed, test_file)
        for test_file in test_files
    )


def fitness(test_files, seeds, entity):
    """Fitness is mean score over multiple seeds and test files."""
    return int(np.mean([
        process_test_files(entity, seed, test_files)
        for seed in seeds
    ]))


def evaluate_population(test_files, population: list):

    seeds = [random.randint(0, 1000000) for _ in range(N_SEEDS)]

    fitnesses = []
    for i, entity in enumerate(population):
        print(f"Evaluate entity {i} ...", end=" ")
        fitnesses.append(fitness(test_files, seeds, entity))
        print(f"Fitness: {fitnesses[-1]}")

    with open(ENTITIES_FILE, "a") as f:
        # Save all encoutered entities and their fitness
        for i, entity in enumerate(population):
            f.write(', '.join(map(str, entity)) + ', ' + str(fitnesses[i]) + '\n')

    return fitnesses


def select(pop, selection_probs):
    """Weighted random selection proportional to fitness."""
    return random.choices(pop, weights=selection_probs, k=1)[0]


def get_best_entity(population, fitnesses):
    """Return the best genome and its fitness from a population."""
    best_idx = min(range(len(population)), key=lambda i: fitnesses[i])
    return population[best_idx], fitnesses[best_idx]


def crossover(parent1, parent2):
    """Crossover by averaging integers pairwise."""
    return [
        (p1 + p2) // 2 for p1, p2 in zip(parent1, parent2)
    ]


def mutate(genome):
    """Mutate one of the genes randomly."""
    if random.random() < MUTATION_RATE:
        genome[0] = random.randint(*RANGES["entities"])
    if random.random() < MUTATION_RATE:
        genome[1] = random.randint(*RANGES["mr_switch"])
    if random.random() < MUTATION_RATE:
        genome[2] = random.randint(*RANGES["mr_move"])
    if random.random() < MUTATION_RATE:
        genome[3] = random.randint(*RANGES["mr_create"])

    return genome


def genetic_algorithm(test_files):
    # Initialize population
    population = init_population()

    gen = 0
    gen_of_last_best_entity = 0
    overall_best_fitness = 10000000000
    # Stop when no best entity is found in X generations
    while (gen_of_last_best_entity + GA_GEN_LIMIT > gen):

        print(f"\nStart generation {gen} (Last best entity was in gen N-{gen - gen_of_last_best_entity}, over {GA_GEN_LIMIT} max)")
        fitnesses = evaluate_population(test_files, population)

        # Catch the best entity
        best_genome, best_fitness = get_best_entity(population, fitnesses)
        print(f"Gen {gen}: Best genome {best_genome}: Fitness {best_fitness}")

        if best_fitness < overall_best_fitness:
            overall_best_fitness = best_fitness
            gen_of_last_best_entity = gen

            # Save each new best entity
            with open(BEST_ENTITIES_FILE, "a") as f:
                f.write(', '.join(map(str, best_genome)) + ', ' + str(best_fitness) + '\n')

        # Create next generation
        fitness_max = max(fitnesses)
        selection_probs = [fitness_max - s for s in fitnesses]

        new_population = [best_genome]
        while len(new_population) < POPULATION_SIZE:
            parent1 = select(population, selection_probs)
            parent2 = select(population, selection_probs)

            child = crossover(parent1, parent2)
            child = mutate(child)

            new_population.append(child)

        population = new_population
        gen += 1


if __name__ == "__main__":

    test_files = get_test_files_from_folder("testset")

    # Write all the constants and their result in a file
    # with open(ENTITIES_FILE, "w") as f:
    #     f.write(f"entities, mr_switch, mr_move, mr_create, result\n")
    # with open(BEST_ENTITIES_FILE, "w") as f:
    #     f.write(f"entities, mr_switch, mr_move, mr_create, result\n")

    genetic_algorithm(test_files)
