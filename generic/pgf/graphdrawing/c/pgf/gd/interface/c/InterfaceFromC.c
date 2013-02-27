
// Own header:
#include <pgf/gd/interface/c/InterfaceFromC.h>

// Lua stuff:
#include <lauxlib.h>

// C stuff:
#include <stdlib.h>
#include <string.h>



// Help functions

static void set_field (lua_State* L, const char* what, const char* where)
{
  if (what) {
    lua_pushstring(L, what);
    lua_setfield(L, -2, where);
  }
}


static void init_coordinate_array(pgfgd_Coordinate_array* a, int count)
{
  a->length = count;
  a->array  = (pgfgd_Coordinate*) calloc(count, sizeof(pgfgd_Coordinate));
}

static void init_vertex_array(pgfgd_Vertex_array* a, int count)
{
  a->length = count;
  a->array  = (pgfgd_Vertex**) calloc(count, sizeof(pgfgd_Vertex*));
}

static void init_edge_array(pgfgd_Edge_array* a, int count)
{
  a->length = count;
  a->array  = (pgfgd_Edge**) calloc(count, sizeof(pgfgd_Edge*));
}

static void init_arcs_array(pgfgd_Arc_array* arcs, int length)
{
  arcs->length = length;
  arcs->tails  = (int*) calloc(length, sizeof(int));
  arcs->heads  = (int*) calloc(length, sizeof(int));
}

static void init_path_array(pgfgd_path_field_array* a, int count)
{
  a->length      = count;
  a->coordinates = (pgfgd_Coordinate*) calloc(count, sizeof(pgfgd_Coordinate));
  a->strings     = (char**) calloc(count, sizeof(char*));
}



// Option handling

struct pgfgd_OptionTable {
  lua_State* state;

  int kind;
  int index;
};

#define GRAPH_INDEX 1
#define VERTICES_INDEX 2
#define EDGES_INDEX 3
#define ALGORITHM_INDEX 4

#define BACKINDEX_STORAGE_INDEX 5

#define FUNCTION_UPVALUE 1
#define DIGRAPH_OBJECT_UPVALUE 2


static pgfgd_OptionTable* make_option_table(lua_State* L, int kind, int index)
{
  pgfgd_OptionTable* t = (pgfgd_OptionTable*) malloc(sizeof(pgfgd_OptionTable));

  t->state = L;
  t->kind = kind;
  t->index = index;

  return t;
}

static void push_option_table(pgfgd_OptionTable* t)
{
  switch (t->kind) {
  case GRAPH_INDEX:
    lua_getfield(t->state, GRAPH_INDEX, "options");
    break;
  case VERTICES_INDEX:
    lua_rawgeti(t->state, VERTICES_INDEX, t->index);
    lua_getfield(t->state, -1, "options");
    lua_replace(t->state, -2);
    break;
  case EDGES_INDEX:
    lua_rawgeti(t->state, EDGES_INDEX, t->index);
    lua_getfield(t->state, -1, "options");
    lua_replace(t->state, -2);
    break;
  }
}

int pgfgd_isset(pgfgd_OptionTable* t, const char* key)
{
  push_option_table(t);
  lua_getfield(t->state, -1, key);
  int is_nil = lua_isnil(t->state, -1);
  lua_pop(t->state, 2);
  return !is_nil;
}

int pgfgd_isnumber(pgfgd_OptionTable* t, const char* key)
{
  push_option_table(t);
  lua_getfield(t->state, -1, key);
  int is_number = lua_isnumber(t->state, -1);
  lua_pop(t->state, 2);
  return is_number;
}

int pgfgd_isboolean(pgfgd_OptionTable* t, const char* key)
{
  push_option_table(t);
  lua_getfield(t->state, -1, key);
  int is_bool = lua_isboolean(t->state, -1);
  lua_pop(t->state, 2);
  return is_bool;
}

int pgfgd_isstring(pgfgd_OptionTable* t, const char* key)
{
  push_option_table(t);
  lua_getfield(t->state, -1, key);
  int is_string = lua_isstring(t->state, -1);
  lua_pop(t->state, 2);
  return is_string;
}

int pgfgd_isuser(pgfgd_OptionTable* t, const char* key)
{
  push_option_table(t);
  lua_getfield(t->state, -1, key);
  int is_user = lua_isuserdata(t->state, -1);
  lua_pop(t->state, 2);
  return is_user;
}


double pgfgd_tonumber(pgfgd_OptionTable* t, const char* key)
{
  push_option_table(t);
  lua_getfield(t->state, -1, key);
  double d = lua_tonumber(t->state, -1);
  lua_pop(t->state, 2);
  return d;
}

int pgfgd_toboolean(pgfgd_OptionTable* t, const char* key)
{
  push_option_table(t);
  lua_getfield(t->state, -1, key);
  int d = lua_toboolean(t->state, -1);
  lua_pop(t->state, 2);
  return d;
}

char* pgfgd_tostring(pgfgd_OptionTable* t, const char* key)
{
  push_option_table(t);
  lua_getfield(t->state, -1, key);
  const char* s = lua_tostring(t->state, -1);
  char* copy = strcpy((char*) malloc(strlen(s)+1), s);
  lua_pop(t->state, 2);
  return copy;
}

void* pgfgd_touser(pgfgd_OptionTable* t, const char* key)
{
  push_option_table(t);
  lua_getfield(t->state, -1, key);
  void* d = lua_touserdata(t->state, -1);
  lua_pop(t->state, 2);
  return d;
}


// Handling algorithms

struct pgfgd_SyntacticDigraph_internals {
  lua_State* state;
};

static char* make_string_from(lua_State* L, const char* name)
{
  lua_getfield(L, -1, name);
  if (lua_isnil(L, -1)) {
    // Field not set; return emtpy string.
    lua_pop(L, 1);
    return (char*) calloc(1, sizeof(char));
  }
  else {
    const char* s = lua_tostring(L, -1);
    char* copy = strcpy((char*) malloc(strlen(s)+1), s);
    lua_pop(L, 1);
    return copy;
  }
}

static void make_coordinate(lua_State* L, pgfgd_Coordinate* c)
{
  lua_getfield(L, -1, "x");
  c->x = lua_tonumber(L, -1);
  lua_pop(L, 1);
  
  lua_getfield(L, -1, "y");
  c->y = lua_tonumber(L, -1);
  lua_pop(L, 1);
}

static void construct_digraph(lua_State* L, pgfgd_SyntacticDigraph* d)
{
  d->internals = (pgfgd_SyntacticDigraph_internals*) calloc(1, sizeof(pgfgd_SyntacticDigraph_internals));
  d->internals->state = L;
  
  // Create the options table:
  d->options = make_option_table(L, GRAPH_INDEX, 0);

  // Create the vertex table
  init_vertex_array(&d->vertices, lua_objlen(L, VERTICES_INDEX));

  // Create the vertices
  int i;
  for (i=0; i<d->vertices.length; i++) {
    pgfgd_Vertex* v  = (pgfgd_Vertex*) calloc(1, sizeof(pgfgd_Vertex));

    // Push the vertex onto the Lua stack:
    lua_rawgeti(L, VERTICES_INDEX, i+1);
    
    // Fill v with information:
    v->name  = make_string_from(L, "name");
    v->shape = make_string_from(L, "shape");
    v->kind  = make_string_from(L, "kind");
    
    // Options:
    v->options = make_option_table(L, VERTICES_INDEX, i+1);

    // Setup pos field
    lua_getfield(L, -1, "pos");
    make_coordinate(L, &v->pos);
    lua_pop(L, 1);
    
    // Setup hull_center
    lua_getfield(L, -1, "hull_center");
    make_coordinate(L, &v->hull_center);
    lua_pop(L, 1);

    // Setup hull array:
    lua_getfield(L, -1, "hull");
    init_coordinate_array(&v->hull, lua_objlen(L, -1));
    int j;
    for (j=0; j<v->hull.length; j++) {
      lua_rawgeti(L, -1, j+1);
      make_coordinate(L, &v->hull.array[j]);
      lua_pop(L, 1);
    }
    lua_pop(L, 1); // The hull array

    // Pop the vertex
    lua_pop(L, 1);
    
    d->vertices.array[i] = v;
  }

  // Construct the edges:
  init_edge_array(&d->syntactic_edges, lua_objlen(L, EDGES_INDEX));

  int edge_index;
  for (edge_index = 0; edge_index < d->syntactic_edges.length; edge_index++) {
    pgfgd_Edge* e = (pgfgd_Edge*) calloc(1, sizeof(pgfgd_Edge));

    lua_rawgeti(L, EDGES_INDEX, edge_index+1);
    
    e->direction = make_string_from(L, "direction");      
    e->options = make_option_table(L, EDGES_INDEX, edge_index+1);

    // Compute the tail vertex index:
    lua_getfield(L, -1, "tail");
    lua_gettable(L, VERTICES_INDEX);
    e->tail = d->vertices.array[lua_tointeger(L, -1) - 1];
    lua_pop(L, 1);

    lua_getfield(L, -1, "head");
    lua_gettable(L, VERTICES_INDEX);
    e->head = d->vertices.array[lua_tointeger(L, -1) - 1];
    lua_pop(L, 1);
    
    // Fill path:
    lua_getfield(L, -1, "path");
    int path_length = lua_objlen(L, -1);
    if (path_length > 0) {
      init_path_array(&e->path, path_length);
      int i;
      for (i = 0; i < path_length; i++) {
	lua_rawgeti(L, -1, i+1);
	if (lua_isstring(L, -1)) {
	  const char* s = lua_tostring(L, -1);
	  e->path.strings[i] = strcpy((char*) malloc(strlen(s)+1), s);
	} else {
	  lua_getfield(L, -1, "x");
	  e->path.coordinates[i].x = lua_tonumber(L, -1);
	  lua_pop(L, 1);
	  
	  lua_getfield(L, -1, "y");
	  e->path.coordinates[i].y = lua_tonumber(L, -1);
	  lua_pop(L, 1);
	}
	lua_pop(L, 1);
      }       
    }
    lua_pop(L, 1);
    
    // Pop the edge form the Lua stack:
    lua_pop(L, 1);
      
    d->syntactic_edges.array[edge_index] = e;
  }
}


static void sync_digraph(lua_State* L, pgfgd_SyntacticDigraph* d)
{
  // Writes back the computed position information to the digraph:
  int i;
  for (i=0; i<d->vertices.length; i++) {

    // Push pos field of vertex:
    lua_rawgeti(L, VERTICES_INDEX, i+1);
    lua_getfield(L, -1, "pos");

    // Set x and y coordinates:
    pgfgd_Vertex* v = d->vertices.array[i]; 
    lua_pushnumber(L, v->pos.x);
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, v->pos.y);
    lua_setfield(L, -2, "y");

    // pop pos and vertex
    lua_pop(L, 2);
  }

  // Write back the paths
  
  // First, get "Coordinate.new" as a "local"
  lua_getglobal(L, "require");
  lua_pushstring(L, "pgf.gd.model.Coordinate");
  lua_call(L, 1, 1);
  lua_getfield(L, -1, "new");
  int new_fun_index = lua_gettop(L);
  
  for (i=0; i < d->syntactic_edges.length; i++) {
    pgfgd_Edge* e = d->syntactic_edges.array[i];

    lua_rawgeti(L, EDGES_INDEX, i+1);
    lua_createtable(L, e->path.length ? e->path.length : 1, 0);

    int j;
    for (j=0; j<e->path.length; j++) {
      if (e->path.strings[j]) {
	lua_pushstring(L, e->path.strings[j]);
	lua_rawseti(L, -2, j+1);
      } else {
	lua_pushvalue(L, new_fun_index);
	lua_pushnumber(L, e->path.coordinates[j].x);
	lua_pushnumber(L, e->path.coordinates[j].y);
	lua_call(L, 2, 1);
	lua_rawseti(L, -2, j+1);
      }
    }

    lua_setfield(L, -2, "path");
    lua_pop(L, 1);            
  }

  lua_pop(L, 2); // new 
}


static void free_digraph(pgfgd_SyntacticDigraph* digraph)
{
  int i;
  for (i=0; i < digraph->vertices.length; i++) {
    pgfgd_Vertex* v = digraph->vertices.array[i];
    
    free(v->hull.array);
    free(v->name);
    free(v->shape);
    free(v->kind);
    free(v->options);
    free(v->incoming.array);
    free(v->outgoing.array);
    free(v);
  }
  
  for (i=0; i < digraph->syntactic_edges.length; i++) {
    pgfgd_Edge* e = digraph->syntactic_edges.array[i];
    
    int j;
    for (j=0; j < e->path.length; j++)
      free(e->path.strings[j]);
    
    free(e->path.coordinates);
    free(e->path.strings);
    free(e->direction);
    free(e->options);
    free(e);
  }
  
  free(digraph->vertices.array);
  free(digraph->syntactic_edges.array);  
  free(digraph->options);
  free(digraph->internals);
  free(digraph);
}

static int algorithm_dispatcher(lua_State* L)
{
  // Push the back index table. It will be at index BACKINDEX_STORAGE_INDEX
  lua_createtable(L, 0, 1);
  
  // The actual function is stored in an upvalue.
  pgfgd_SyntacticDigraph* digraph = (pgfgd_SyntacticDigraph*) calloc(1, sizeof(pgfgd_SyntacticDigraph));
  
  construct_digraph(L, digraph);
  
  pgfgd_algorithm_fun fun = lua_touserdata(L, lua_upvalueindex(FUNCTION_UPVALUE));
  fun(digraph);

  sync_digraph(L, digraph);
  
  free_digraph(digraph);

  return 0;
}


void pgfgd_path_clear(pgfgd_Edge* e)
{
  int i;
  for (i=0; i < e->path.length; i++)
    free(e->path.strings[i]);

  free(e->path.strings);
  free(e->path.coordinates);

  e->path.length = 0;
  e->path.strings = 0;
  e->path.coordinates = 0;
}

void pgfgd_path_add_coordinate(pgfgd_Edge* e, double x, double y)
{
  e->path.length++;
  e->path.strings = (char **)
    realloc(e->path.strings, e->path.length*sizeof(char*));
  e->path.coordinates = (pgfgd_Coordinate*)
    realloc(e->path.coordinates, e->path.length*sizeof(pgfgd_Coordinate));
  
  e->path.coordinates[e->path.length-1].x = x;
  e->path.coordinates[e->path.length-1].y = y;
  e->path.strings[e->path.length-1] = 0;
}

void pgfgd_path_add_string(pgfgd_Edge* e, const char* s)
{
  e->path.length++;
  e->path.strings = (char **)
    realloc(e->path.strings, e->path.length*sizeof(char*));
  e->path.coordinates = (pgfgd_Coordinate*)
    realloc(e->path.coordinates, e->path.length*sizeof(pgfgd_Coordinate));
  
  e->path.coordinates[e->path.length-1].x = 0;
  e->path.coordinates[e->path.length-1].y = 0;
  e->path.strings[e->path.length-1] = strcpy((char*) malloc(strlen(s)+1), s);
}


// Handling digraphs

struct pgfgd_Digraph {
  lua_State* state;
  const char* name;
  pgfgd_SyntacticDigraph* syntactic_digraph;
};


pgfgd_Digraph* pgfgd_get_digraph (pgfgd_SyntacticDigraph* g, const char* graph_name)
{
  lua_State* L = g->internals->state;
  
  lua_getfield(L, ALGORITHM_INDEX, graph_name);
  if (lua_isnil(L, -1)) 
    luaL_error(L, "digraph named %s not found in algorithm class", graph_name);
  lua_pop(L, 1);
  
  pgfgd_Digraph* new = (pgfgd_Digraph*) calloc(1, sizeof(pgfgd_Digraph));

  new->state = L;
  new->name = graph_name;
  new->syntactic_digraph = g;
  
  return new;
}

int pgfgd_digraph_num_vertices (pgfgd_Digraph* g)
{
  lua_getfield(g->state, ALGORITHM_INDEX, g->name);
  lua_getfield(g->state, -1, "vertices");
  int num = lua_objlen(g->state, -1);
  lua_pop(g->state, 2);

  return num;
}

static void push_digraph_and_backindex(pgfgd_Digraph* g)
{
  lua_State* L = g->state;
  
  // Get digraph
  lua_getfield(L, ALGORITHM_INDEX, g->name);
  int digraph_pos = lua_gettop(L);
  
  // Get entry in back index table
  lua_pushvalue(L, digraph_pos);    
  lua_gettable(L, BACKINDEX_STORAGE_INDEX);
  int backtable_pos = lua_gettop(L);
  if (lua_isnil(L, backtable_pos)) {
    // Aha. We need to install a new table:
    lua_pop(L, 1); // Get rid of nil

    lua_createtable(L, 0, 1);
    
    lua_pushvalue(L, digraph_pos); // The digraph object
    lua_pushvalue(g->state, backtable_pos); // The new table
    lua_settable(L, BACKINDEX_STORAGE_INDEX);

    // Fill the table:
    lua_getfield(L, digraph_pos, "vertices");
    int vertex_pos = lua_gettop(L);
    int i;
    int n = lua_objlen(L, -1);
    for (i = 1; i <= n; i++) {
      lua_rawgeti(L, vertex_pos, i);
      lua_pushinteger(L, i);
      lua_settable(L, backtable_pos);
    }
    lua_pop(L, 1);
  }
}

static pgfgd_Arc_array* build_c_array_of_arcs_from_lua_array_of_arcs(pgfgd_Digraph* g)
{
  lua_State* L = g->state;

  pgfgd_Arc_array* arcs = (pgfgd_Arc_array*) calloc(1, sizeof(pgfgd_Arc_array));
  
  int array_pos = lua_gettop(L);
  
  push_digraph_and_backindex(g);
  int backtable_pos = lua_gettop(L);  

  // Get number of arcs:
  init_arcs_array(arcs, lua_objlen(L, array_pos));
  int i;
  for (i = 1; i<=arcs->length; i++) {
    lua_rawgeti(L, array_pos, i); // Get arcs[i]
    
    // The tail field:
    lua_getfield(L, -1, "tail");
    lua_gettable(L, backtable_pos);
    
    arcs->tails[i-1] = lua_tointeger(L, -1);
    lua_pop(L, 1); // The number
    
    // The head field:
    lua_getfield(L, -1, "head");
    lua_gettable(L, backtable_pos);
    arcs->heads[i-1] = lua_tointeger(L, -1);
    lua_pop(L, 1); // The number
    
    lua_pop(L, 1); // Pop arcs[i]
  }
  
  lua_settop(L, array_pos);

  return arcs;
}


pgfgd_Arc_array* pgfgd_digraph_arcs (pgfgd_Digraph* g)
{
  lua_getfield(g->state, ALGORITHM_INDEX, g->name);
  lua_getfield(g->state, -1, "arcs");

  pgfgd_Arc_array* a = build_c_array_of_arcs_from_lua_array_of_arcs(g);

  lua_pop(g->state, 2);

  return a;
}


pgfgd_Vertex* pgfgd_digraph_syntactic_vertex (pgfgd_Digraph* g, int v)
{
  int tos = lua_gettop(g->state);
  pgfgd_Vertex* return_me = 0;
  
  lua_getfield(g->state, ALGORITHM_INDEX, g->name);
  lua_getfield(g->state, -1, "vertices");
  lua_rawgeti(g->state, -1, v);
  
  lua_gettable(g->state, VERTICES_INDEX);
  if (lua_isnumber(g->state, -1)) {
    return_me = g->syntactic_digraph->vertices.array[lua_tointeger(g->state, -1)-1];
  }
  
  lua_settop(g->state, tos);
  return return_me;
}


int pgfgd_digraph_isarc (pgfgd_Digraph* g, int tail, int head)
{
  lua_State* L = g->state;
  int is_arc = 0;
  
  lua_getfield(L, ALGORITHM_INDEX, g->name);
  int digraph_pos = lua_gettop(L);
  
  lua_getfield(L, digraph_pos, "vertices");
  int vertices_pos = lua_gettop(L);

  // Call digraph:connected(tail, head)
  lua_pushvalue(L, lua_upvalueindex(DIGRAPH_OBJECT_UPVALUE));
  lua_getfield(L, -1, "arc");
  lua_pushvalue(L, digraph_pos);
  lua_rawgeti(L, vertices_pos, tail);
  lua_rawgeti(L, vertices_pos, head);
  lua_call(L, 3, 1);
  
  is_arc = !lua_isnil(L, -1);

  lua_settop(L, digraph_pos-1);
  
  return is_arc;
}

pgfgd_Arc_array* pgfgd_digraph_incoming (pgfgd_Digraph* g, int v)
{
  lua_State* L = g->state;
  
  lua_getfield(L, ALGORITHM_INDEX, g->name);
  int digraph_pos = lua_gettop(L);
  
  lua_getfield(L, digraph_pos, "vertices");
  int vertices_pos = lua_gettop(L);

  // Call digraph:connected(tail, head)
  lua_pushvalue(L, lua_upvalueindex(DIGRAPH_OBJECT_UPVALUE));
  lua_getfield(L, -1, "incoming");
  lua_pushvalue(L, digraph_pos);
  lua_rawgeti(L, vertices_pos, v);
  lua_call(L, 2, 1);
  
  pgfgd_Arc_array* a = build_c_array_of_arcs_from_lua_array_of_arcs(g);

  lua_settop(L, digraph_pos-1);
  
  return a;  
}

pgfgd_Arc_array* pgfgd_digraph_outgoing (pgfgd_Digraph* g, int v)
{
  lua_State* L = g->state;
  
  lua_getfield(L, ALGORITHM_INDEX, g->name);
  int digraph_pos = lua_gettop(L);
  
  lua_getfield(L, digraph_pos, "vertices");
  int vertices_pos = lua_gettop(L);

  // Call digraph:connected(tail, head)
  lua_pushvalue(L, lua_upvalueindex(DIGRAPH_OBJECT_UPVALUE));
  lua_getfield(L, -1, "outgoing");
  lua_pushvalue(L, digraph_pos);
  lua_rawgeti(L, vertices_pos, v);
  lua_call(L, 2, 1);
  
  pgfgd_Arc_array* a = build_c_array_of_arcs_from_lua_array_of_arcs(g);

  lua_settop(L, digraph_pos-1);
  
  return a;  
}

pgfgd_Edge_array* pgfgd_digraph_syntactic_edges  (pgfgd_Digraph* g, int tail, int head)
{
  pgfgd_Edge_array* edges = (pgfgd_Edge_array*) calloc(1, sizeof(pgfgd_Edge_array));

  // First, get the arc in the syntatic digraph.
  lua_State* L = g->state;
  int tos = lua_gettop(L);
  
  lua_getfield(L, ALGORITHM_INDEX, g->name);
  int digraph_pos = lua_gettop(L);
  
  lua_getfield(L, digraph_pos, "vertices");
  int vertices_pos = lua_gettop(L);

  // Call digraph:arc(tail, head)
  lua_pushvalue(L, lua_upvalueindex(DIGRAPH_OBJECT_UPVALUE));
  int digraph_class_index = lua_gettop(L);
  
  lua_getfield(L, digraph_class_index, "arc");
  lua_pushvalue(L, digraph_pos);
  lua_rawgeti(L, vertices_pos, tail);
  lua_rawgeti(L, vertices_pos, head);
  lua_call(L, 3, 1);

  if (!lua_isnil(L, -1)) {
    // Get syntactic_digraph of the arc:
    lua_getfield(L, -1, "syntactic_digraph");

    // Call syntactic_digraph:arc(tail, head)
    lua_getfield(L, digraph_class_index, "arc");
    lua_pushvalue(L, -2);
    lua_rawgeti(L, vertices_pos, tail);
    lua_rawgeti(L, vertices_pos, head);
    lua_call(L, 3, 1);

    if (!lua_isnil(L, -1)) {
      lua_getfield(L, -1, "syntactic_edges");
      int syntactic_edges_index = lua_gettop(L);
      
      int n = lua_objlen(L, syntactic_edges_index);
      init_edge_array(edges, n);
      int i;
      for (i=1; i<=n; i++) {
	lua_rawgeti(L, syntactic_edges_index, i);
	lua_gettable(L, EDGES_INDEX);
	if (!lua_isnumber(L, -1)) {
	  pgfgd_digraph_free_edge_array(edges);
	  luaL_error(L, "syntactic edge index not found");
	}
	int p = lua_tonumber(L, -1);
	edges->array[i-1] = g->syntactic_digraph->syntactic_edges.array[p-1];
      }
    }      
  }

  lua_settop(L, tos);
  
  return edges;
}



void pgfgd_digraph_free (pgfgd_Digraph* g)
{
  free(g);
}

void pgfgd_digraph_free_arc_array (pgfgd_Arc_array* arcs)
{
  free (arcs->tails);
  free (arcs->heads);
  free (arcs);
}

void pgfgd_digraph_free_edge_array (pgfgd_Edge_array* edges)
{
  free (edges->array);
  free (edges);
}



// Handling declarations


struct pgfgd_Declaration {
  const char* key;
  const char* summary;
  const char* type;
  const char* initial;
  void* initial_user;
  const char* default_value;
  const char* alias;
  const char* documentation;
  pgfgd_algorithm_fun algorithm;
  const char*         phase;

  int use_length;
  const char** use_keys;
  const char** use_values_strings;
  void**       use_values_user;
  
  int examples_length;
  const char** examples;

  int pre_length;
  const char** pre;

  int post_length;
  const char** post;
};


pgfgd_Declaration* pgfgd_new_key (const char* key)
{
  pgfgd_Declaration* d = (pgfgd_Declaration*) calloc(1, sizeof(pgfgd_Declaration));
  
  d->key = key;
  
  return d;
}

void pgfgd_declare(struct lua_State* state, pgfgd_Declaration* d)
{
  if (d && d->key) {
    int tos = lua_gettop(state);
    
    // Find declare function:
    lua_getglobal(state, "require");
    lua_pushstring(state, "pgf.gd.interface.InterfaceToAlgorithms");
    lua_call(state, 1, 1);
    lua_getfield(state, -1, "declare");
    
    // Build a Lua table:
    lua_createtable(state, 0, 11);

    set_field (state, d->key, "key");
    set_field (state, d->summary, "summary");
    set_field (state, d->type, "type");
    set_field (state, d->initial, "initial");
    set_field (state, d->documentation, "documentation");
    set_field (state, d->default_value, "default");
    set_field (state, d->alias, "alias");
    set_field (state, d->phase, "phase");

    if (d->initial_user) {
      lua_pushlightuserdata(state, d->initial_user);
      lua_setfield(state, -2, "initial");
    }
    
    if (d->use_length > 0) {
      lua_createtable(state, d->use_length, 0);
      int i;
      for (i=0; i < d->use_length; i++) {
	lua_createtable(state, 0, 2);
	set_field(state, d->use_keys[i], "key");
	if (d->use_values_strings[i])
	  set_field(state, d->use_values_strings[i], "value");
	if (d->use_values_user[i]) {
	  lua_pushlightuserdata(state, d->use_values_user[i]);
	  lua_setfield(state, -2, "value");
	}
	lua_rawseti(state, -2, i+1);
      }
      lua_setfield(state, -2, "use");
    }
    
    if (d->pre) {
      lua_createtable(state, 0, d->pre_length);
      int i;
      for (i=0; i < d->pre_length; i++) {
	lua_pushboolean(state, 1);
	lua_setfield(state, -2, d->pre[i]);
      }
      lua_setfield(state, -2, "preconditions");
    }
    
    if (d->post) {
      lua_createtable(state, 0, d->post_length);
      int i;
      for (i=0; i < d->post_length; i++) {
	lua_pushboolean(state, 1);
	lua_setfield(state, -2, d->post[i]);
      }
      lua_setfield(state, -2, "postconditions");
    }
    
    if (d->examples) {
      lua_createtable(state, d->examples_length, 0);
      int i;
      for (i=0; i < d->examples_length; i++) {
	lua_pushstring(state, d->examples[i]);
	lua_rawseti(state, -2, i+1);
      }
      lua_setfield(state, -2, "examples");
    }

    if (d->algorithm) {
      // The algorithm function is stored as lightuserdate upvalue 
      lua_pushlightuserdata(state, (void *) d->algorithm);

      // Find the Digraph.arc function and store it as the second upvalue:
      lua_getglobal(state, "require");
      lua_pushstring(state, "pgf.gd.model.Digraph");
      lua_call(state, 1, 1);
      
      lua_pushcclosure(state, algorithm_dispatcher, 2);
      lua_setfield(state, -2, "algorithm_written_in_c");
    }

    // Call the declare function:
    lua_call(state, 1, 0);

    // Cleanup:
    lua_settop(state, tos);
  }
}

void pgfgd_key_add_use(pgfgd_Declaration* d, const char* key, const char* value)
{
  d->use_length++;
  d->use_keys = (const char **) realloc(d->use_keys, d->use_length*sizeof(const char*));
  d->use_values_strings = (const char **) realloc(d->use_values_strings, d->use_length*sizeof(const char*));
  d->use_values_user = (void **) realloc(d->use_values_user, d->use_length*sizeof(void*));
  
  d->use_keys          [d->use_length-1] = key;
  d->use_values_strings[d->use_length-1] = value;  
  d->use_values_user   [d->use_length-1] = 0;  
}

void pgfgd_key_add_use_user(pgfgd_Declaration* d, const char* key, void* value)
{
  d->use_length++;
  d->use_keys = (const char **) realloc(d->use_keys, d->use_length*sizeof(const char*));
  d->use_values_strings = (const char **) realloc(d->use_values_strings, d->use_length*sizeof(const char*));
  d->use_values_user = (void **) realloc(d->use_values_user, d->use_length*sizeof(void*));
  
  d->use_keys          [d->use_length-1] = key;
  d->use_values_strings[d->use_length-1] = 0;  
  d->use_values_user   [d->use_length-1] = value;  
}

void pgfgd_key_add_example(pgfgd_Declaration* d, const char* s)
{
  d->examples_length++;
  d->examples = (const char **) realloc(d->examples, d->examples_length*sizeof(const char*));
  
  d->examples[d->examples_length-1] = s;
}

void pgfgd_key_add_precondition(pgfgd_Declaration* d, const char* s)
{
  d->pre_length++;
  d->pre = (const char **) realloc(d->pre, d->pre_length*sizeof(const char*));
  
  d->pre[d->pre_length-1] = s;
}

void pgfgd_key_add_postcondition(pgfgd_Declaration* d, const char* s)
{
  d->post_length++;
  d->post = (const char **) realloc(d->post, d->post_length*sizeof(const char*));
  
  d->post[d->post_length-1] = s;
}

void pgfgd_key_summary(pgfgd_Declaration* d, const char* s)
{
  d->summary = s;
}

void pgfgd_key_type(pgfgd_Declaration* d, const char* s)
{
  d->type = s;
}

void pgfgd_key_initial(pgfgd_Declaration* d, const char* s)
{
  d->initial = s;
}

void pgfgd_key_initial_user(pgfgd_Declaration* d, void* s)
{
  d->initial_user = s;
}

void pgfgd_key_default(pgfgd_Declaration* d, const char* s)
{
  d->default_value = s;
}

void pgfgd_key_alias(pgfgd_Declaration* d, const char* s)
{
  d->alias = s;
}

void pgfgd_key_documentation(pgfgd_Declaration* d, const char* s)
{
  d->documentation = s;
}

void pgfgd_key_phase(pgfgd_Declaration* d, const char* s)
{
  d->phase = s;
}

void pgfgd_key_algorithm(pgfgd_Declaration* d, pgfgd_algorithm_fun f)
{
  d->algorithm = f;
}

void pgfgd_free_key(pgfgd_Declaration* d)
{
  if (d) {
    free(d->examples);
    free(d->use_keys);
    free(d->use_values_user);
    free(d->use_values_strings);
    free(d);    
  }
}
