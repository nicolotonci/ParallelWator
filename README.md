# DistributedWator

The project consist of a parallel Wa-Tor simulation.

Wa-Tor is a simple approximation of biological systems, simulating the interaction of predators and prey in the wild. The simulation task takes place on a 2D board where movement wraps around screen edges. The board is randomly filled with fish and sharks at the start. Then at each time step the fish move randomly to an adjacent spot and the sharks try to eat a random adjacent fish or move randomly to an adjacent spot if there is no adjacent fish. Fish and sharks that live for a certain amount of time can breed. Additionally, if a shark does not eat a fish in a certain amount of time it will starve to death.