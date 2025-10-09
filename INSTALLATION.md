## Installation

1. Navigate to your desired directory.

  ```
      cd ~/path/to/directory
  ```


2. Clone this repo:

  ```bash
      git clone git@github.com:mlai-bonn/GEDPaths.git
  ```

3. Install the libGraph library in the same directory (otherwise you have to change the paths in the code)
   
  ```bash 
     git clone git@github.com:mlai-bonn/libGraph.git
  ```
4. Install the GEDLIB **(Instructions for Unix)** (See [gedlib](https://github.com/dbblumenthal/gedlib) for the details.)
    - Navigate to the external folder of libGraph:
      ```bash
      cd libGraph/external
      ```
    - Clone the GEDLIB repo:
      ```bash
      git clone git@github.com:dbblumenthal/gedlib.git
      ```
    - Install Cmake 
      ```bash
      sudo apt-get install cmake
      ```
    - Install Doxygen
      ```bash
      sudo apt-get install doxygen
      ```
    - Only under MacOS: Install OpenMP
    - Use eigen 3.4.0 instead of the default version:
      ```bash
      cd gedlib/ext
      git clone https://gitlab.com/libeigen/eigen.git
      cd eigen
      git checkout 3.4.0
      ```
    - Use boost 1.89.0 instead of the default one
      ```bash
      cd ..
      wget https://archives.boost.io/release/1.89.0/source/boost_1_89_0.tar.bz2
      tar --bzip2 -xf boost_1_89_0.tar.bz2
      rm boost_1_89_0.tar.bz2
      mv boost_1_89_0 boost
      ```
    - For the exact solvers please install GUROBI (see [Gurobi Installation](https://support.gurobi.com/hc/en-us/articles/4534161999889-How-do-I-install-Gurobi-Optimizer))
    - Installing GEDLIB using
      ```bash
      python install.py [--gurobi <GUROBI_ROOT>]
      ```      




