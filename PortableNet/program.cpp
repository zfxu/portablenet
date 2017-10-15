//
//  network.cpp
//  PortableNet
//
//  Created by Andrea Vedaldi on 15/10/2017.
//  Copyright © 2017 VGG. All rights reserved.
//

#include "program.hpp"
#include "json.hpp"

#include <iostream>
#include <fstream>
#include <memory>
#include <cstdio>

using namespace std ;
using namespace nlohmann ;
using namespace vl ;

// MARK: - Workspace

void Workspace::print() const
{
  cout << "Workspace:" << endl ;
  for (auto const& entry : tensors) {
    auto const& name = entry.first ;
    auto const& tensor = entry.second ;
    cout << "\t" << name << ": [" ;
    for (auto i = tensor.getDimensions() ;
         i != tensor.getDimensions() + tensor.getNumDimensions() ; ++i) {
      cout << *i << " " ;
    }
    cout << "] " ;
    if (tensor.getMemory()) {
      switch (tensor.getDataType()) {
        case VLDT_Double:
          cout << static_cast<double const*>(tensor.getMemory())[0] ;
          break ;
        case VLDT_Float:
          cout << static_cast<float const*>(tensor.getMemory())[0] ;
          break ;
        case VLDT_Char:
          cout << static_cast<char const*>(tensor.getMemory())[0] ;
          break ;
      }
      cout << " ..." ;
    } else {
      cout << "<No Data>" ;
    }
    cout << endl ;
  }
}

vl::Tensor Workspace::get(string name)
{
  // Search if such a tensor exists.
  auto const& found = tensors.find(name) ;
  if (found != tensors.end()) { return found->second ; }
  return Tensor() ; // Null tensor
}

vl::Tensor Workspace::get(string name, DataType dt, TensorShape const& shape)
{
  // Search if such a tensor exists.
  auto const& found = tensors.find(name) ;
  if (found != tensors.end()) {
    auto& tensor = found->second ;
    if (tensor.getDataType() == dt && tensor == shape) {
      return tensor ;
    }
  }

  // No matching tensor found; create a new one.
  remove(name) ;
  size_t numBytes = shape.getNumElements() * getDataTypeSizeInBytes(dt) ;
  void* memory = malloc(numBytes) ;
  assert(memory) ;
  Tensor tensor(shape, dt, VLDT_CPU, memory, numBytes) ;

  // Add back to list.
  tensors[name] = tensor ;
  return tensor ;
}

void Workspace::remove(string name)
{
  // Search if such a tensor exists.
  auto const& found = tensors.find(name) ;
  if (found != tensors.end()) {
    auto& tensor = found->second ;
    free(tensor.getMemory()) ;
    tensors.erase(found) ;
  }
}

bool Workspace::exists(string name) const
{
  auto const& found = tensors.find(name) ;
  return (found != tensors.end()) ;
}

Workspace::~Workspace()
{
  for (auto x = tensors.begin() ; x != tensors.end() ; x = tensors.begin()) {
    remove(x->first) ;
  }
}

std::string const& Workspace::baseName() const
{
  return baseNameString ;
}

void Workspace::baseName(std::string const& name)
{
  baseNameString = name ;
}

// MARK: - Commands

void Load(nlohmann::json const& op, Workspace& ws)
{
  // Todo: error checking, gracefult exit, throw exceptions?
  assert(op["inputs"].is_array()) ;
  assert(op["outputs"].is_array()) ;
  assert(op["fileName"].is_string()) ;
  assert(op["dataType"].is_string()) ;
  assert(op["shape"].is_array()) ;

  // Get the name of the output tensor.
  auto name = op["outputs"][0].get<string>() ;

  // Use tensor cached value if any.
  // Todo: this cannot be right in general.
  if (ws.exists(name)) { return ; }

  // Get the tensor data type.
  DataType dt ;
  if (op["dataType"] == "single") {
    dt = VLDT_Float ;
  } else if (op["dataType"] == "double") {
    dt = VLDT_Double ;
  } else {
    assert(false) ;
  }

  // Get the tensro dimensions.
  auto dims = op["shape"].get<vector<size_t>>() ;
  TensorShape shape(dims);

  // Allocate the tensor in the workspace.
  auto tensor = ws.get(name, dt, shape) ;

  // Read the tensor file.
  auto tensorPath = ws.baseName() + "/" + op["fileName"].get<string>() ;
  auto tensorFile = ifstream(tensorPath, ios::in | ios::binary) ;
  if (tensorFile.is_open()) {
    // Todo: endian.
    tensorFile.read(reinterpret_cast<char*>(tensor.getMemory()),
                    shape.getNumElements() * getDataTypeSizeInBytes(dt)) ;
  }
  // Todo: check the file was correctly read.
}

// MARK: - Program

void Program::execute(Workspace& ws)
{
  for (auto const& op : source["operations"]) {
    auto type = op["type"].get<string>() ;
    cout << "Executing " << type << endl ;
    if (type == "Load") {
      Load(op, ws) ;
    }
  }
}

void Program::load(std::string fileName)
{
  // Open the JSON file describing the neural network.
  auto jsonFileName = fileName + "/net.json" ;
  auto jsonFile = ifstream(jsonFileName) ;
  if (!jsonFile.is_open()) {
    return ;
  }

  // Parse the JSON file.
  source.clear() ;
  jsonFile >> source ;

  // Todo: catch errors.
}

void Program::print() const
{
  cout << source.dump(4) << endl ;
}



