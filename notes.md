# Just a few notes on the main concepts and their implementation

### User / test side

- `dir_type`: extends gt::direction (which is basically an array of coordinates) by adding a unique map to an 1D indexes and a method to invert the direction;
- `data_descriptor`: provide a method to span over the range of data in a given dimension:
  - my idea is that it will be masked by the further information on the halos;
  - `== WARN ==`: there is still a doubt here: is it supposed to contain information on the halos or not?
  - `== WARN ==`: it is then initialized only with the size of one dimension: why?;

  - `== WARN ==`: and why the dimension is initialized with 3?

- `neighbor_generator`: returns an array of neighbors, i.e. an array of the type: `std::array<std::pair<id_type, dir_type>, 4>`, where 4 is the number of neighbors for a given example;
  - in `generic_interfaces.cpp` it returns an array of 4 neighbors;

- `iteration spaces`:
  - an iteration space for a communication object is a function that takes the local id and the remote id and return the region to pack/unpack;
  - `== WARN == `: why - int the user code - the second typename template is not specified?

### Implementation side

- `dimension descriptor`: describes the way a dimension is splitted, and for this purpose contains also two arrays of three halo ranges (inner and outer range), which represents the two set halos used respectively for the send and the receive;

- `halo ranges`:
  - range in which a halo spans (on a single dimension);
  - `inner_range(int)` and `outer_range(int)`: why index + 1?

- `topology_t`:
  - is a map from a domain ID to a pair made of:
    - neighbors list;
    - unique id of the domain;
  - topology size is the same as the domain size;

- `generic_pg`:
  - contains only topological information on the neighbors of a domain;
  - provides a target constructor to fill the topology, i.e. define a hash map with the list of neighbors of each id; the target constructor returns a topology;
  - it also has a unique identifier (atomic counter) and even a check if this is the same for all the ranks: `== WARN ==` why? In which cases are we supposed to use more than one processing grid within the same application?
  - `== WARN == ` (ref. line 204): why the assignment is performed in this way?
  - anyway, here it is just performing the assignation of an unique ID;
  - also defined (in the constructor);
    - an `int` `vector` with the sizes of all ranks;
    - an `int` `vector` with the offsets of all ranks;
    - a vector for all ids;
  - `== WARN == `: check the fix that is necessary for the topology indexes;

- `halo_sizes`:
  - describes halo sizes (2 sizes per direction) and provides a `dimension descriptor` getter which gives the dimension size removing the halo size from both ends;

- `partitioned`:
  - list of partitioned dimensions;
  - just an array of dimension indexes;

- `regular_grid_descriptor`:
  - `inner_iteration_space` (same for outer):
    - use the `data descriptor` and the direction;
    - the idea is to first create an iteration space without considering the halos:
      - this is obtained using `make_range_of_data`:
        - uses the `data_descriptor` and a sequence of integer of the rank of the data descriptor obtained with `meta::make_integer_sequence`;
        - here, the keyword `template` is used to specify that `data` is a template.
      - after that, `make_tuple_of_inner_ranges` is called in order to specify the ranges for the partitioned directions (the full data range is substituted with one of the inner halo ranges);
        - which halo range is decided accordingly to the direction;
        - `make_tuple_of_inner_ranges` is recursive, and the last time (when it is called with no partition indexes), simply data_range is returned;
- `generic_co`:
  - has an id, a processing grid, and two iteriation spaces, one for the send and one for the receive;
  - each id will then have its own communicator;
  - defines a `future` structure, with the `wait()` function;
  - `== WARN ==`: why copy constructor is deleted?
  - `== WARN ==`: `typename std::remove_all_extents<typename std::remove_pointer<D>::type>::type`; this is because the data for now are just a simple c pointer. So we want to access directly the value type without thinking of the data structure. The idea is to have then a more complex data structure, not only a descriptor for the data ranges but also a descriptor for the value with its own data type;
  - `exchange`: performs both the receive and the send operation for all the neighbors of a given ID.
