import subprocess
import os
from typing import Generator

import numpy as np


def run_test_with_config(cpp_executable: str, config_integers: tuple, seed: int, file_name: str):
    """Exécute le test avec une GA constants donnée et retourne le résultat."""

    # print(f"Run '{cpp_executable}' with MR '{config_integers}' and test file '{file_name}' ...")
    config_integers = list(config_integers) + [seed]

    # Lire le contenu du fichier de test
    with open(file_name, 'r') as f:
        content = f.read()

    # Préfixer avec les entiers de GA constants
    prefixed_content = ' '.join(map(str, config_integers)) + '\n' + content

    # Écrire dans un fichier temporaire
    temp_file = 'temp_input.txt'
    with open(temp_file, 'w') as f:
        f.write(prefixed_content)

    # Appeler l'exécutable C++ avec le fichier temporaire comme entrée
    result = subprocess.run(
        [cpp_executable],
        stdin=open(temp_file, 'r'),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )

    # Supprimer le fichier temporaire
    os.remove(temp_file)

    # Récupérer et convertir le résultat
    try:
        output = int(result.stdout.strip())
    except Exception as e:
        print(f"Erreur de conversion pour {file_name}: {result.stdout}")
        print(e)
        raise e

    return output


def process_test_files(cpp_executable: str, ga_constants: tuple, seed: int, test_files: list[str]):
    """Traite tous les fichiers de test avec une GA constants donnée et retourne la somme des résultats."""
    return sum(
        run_test_with_config(cpp_executable, ga_constants, seed, test_file)
        for test_file in test_files
    )


def generate_mutation_rates() -> Generator:
    """Génère une liste de n entiers aléatoires entre min_val et max_val."""

    yield (50, 10, 10, 1)
    yield (400, 20, 20, 1)

    # for n_entities in range(50, 500, 100):
    #     for mr_switch in range(5, 40, 10):
    #         for mr_move in range(5, 40, 10):
    #             for mr_create in range(1, 2):
    #                 yield (
    #                     n_entities, mr_switch, mr_move, mr_create
    #                 )


def main(cpp_executable: str, test_files: list[str]):
    """Exécute le processus pour plusieurs combinaisons de GA constants."""
    seeds = [42, 101, 314]

    best_ga_constants = None
    best_sum = float('inf')
    results = []
    for ga_constants in generate_mutation_rates():

        # print(f"Run '{cpp_executable}' with MR '{ga_constants}' and {len(seeds)} seeds ...")

        # Run the test files with this ga_constants 3 times with different seeds
        dist_sum = int(np.mean(
            [
                process_test_files(cpp_executable, ga_constants, seed, test_files)
                for seed in seeds
            ]
        ))

        results.append((ga_constants, dist_sum))

        if dist_sum < best_sum:
            best_sum = dist_sum
            best_ga_constants = ga_constants
            print(f"GA constants: {ga_constants} → Résultat: {dist_sum} (New bests !)")
        else:
            print(f"GA constants: {ga_constants} → {dist_sum} (Best are: {best_ga_constants} → {best_sum})")

    print(f"The overall best GA constants were {best_ga_constants} with a total sum of {best_sum}")

    # Trier les résultats par ordre croissant de dist_sum
    results.sort(key=lambda x: x[1])

    # Write all the constants and their result in a file
    with open("ga_params_finetuning_results.csv", "w") as f:
        f.write(f"entities, mr_switch, mr_move, mr_create, result\n")
        for ga_constants, dist_sum in results:
            f.write(f"{ga_constants[0]}, {ga_constants[1]}, {ga_constants[2]}, {ga_constants[3]}, {dist_sum}\n")


def get_test_files_from_folder(folder_path: str) -> list[str]:
    """Récupère la liste des fichiers .txt dans un dossier."""
    test_files = []
    for file in os.listdir(folder_path):
        test_files.append(os.path.join(folder_path, file))
    return test_files


# Exemple d'utilisation
if __name__ == "__main__":
    test_files = get_test_files_from_folder("testset")

    # Chemin vers l'exécutable C++ (à adapter)
    cpp_executable = "./vehicle_routing"

    main(cpp_executable, test_files)
