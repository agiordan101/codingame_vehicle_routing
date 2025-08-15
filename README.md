# codingame_vehicle_routing

# Versions

## 1.4 - 141 484

- New mutation : Create a ride with a random customer from another ride
- Fix mutation bug in move_customer() where a customer deappeared

## 1.3 - 135 981

- Fix random number generation for mutations

## 1.2 - 134 495

- Increase population size from 10 to 550
- Increase mutation rates

## 1.1 - 126 110 (Best version)

- New mutation : Move a customer from a ride to insert it in another ride (Can remove a ride)

## 1.0 - 154 957

Start a genetic algorithm.
- Generate a random population
- Select parents weighted by their fitness
- Mutate : Switch two random customers
- Keep best entity

## 0.3 - Best score : 253 442

- Iterate on :
    - Generate a random population
    - Keep best entity based on their fitness

## 0.2 - Best score: 321 374

Customers go in rides in order, or start a new ride.

## 0.1 - Best score: 550 762

One ride per customer

# Next steps

- Add DEBUG mode

- Fix errors about :
    - Some customers have not been served !

- Add mumation :
    - Create a ride from a random customer
