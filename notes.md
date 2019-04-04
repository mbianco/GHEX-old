# Just a few notes on the main concepts and their implementation

### User / test side

- `dir_type`: extends gt::direction by adding a unique map to an 1D indexes and a method to invert the direction;
- `data_descriptor`: provide a method to span over the range of data in a given dimension:
  - my idea is that it will be masked by the further information ont the halos;
  - `== WARN ==`: there is still a doubt here: is it supposed to contain information on the halos or not?
  - it is then initialized only with the size of one dimension: why?;
- `neighbor_generator`: returns an array of neighbors, i.e. an array of the type: `std::array<std::pair<id_type, dir_type>, 4>`, where 4 is the number of neighbors for a given example;
  - in `generic_interfaces.cpp` it returns an array of 4 neighbors;

### Implementation side

- `topology_t`:
  - is a map from a domain ID to a pair made of:
    - neighbors list;
    - unique id of the domain;
  - topology size is the same as the domain size;

- `generic_pg`:
  - contains only topological information on the neighbors of a domain;
  - provides a target constructor to fill the topology, i.e. define a hash map with the list of neighbors of each id; the target constructor returns a topology;
  - it also has a unique identifier (atomic counter) and even a check if this is the same for all the ranks: `== WARN ==` why? In which cases are we supposed to use more than one processing grid within the same application?
