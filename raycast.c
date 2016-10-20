#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int line = 1;

// Holds an rgb triple of a pixel
typedef struct Pixel {
  unsigned char red, green, blue;
} Pixel;

// Holds information about the header of a ppm file
typedef struct Header {
   unsigned char magicNumber;
   unsigned int width, height, maxColor;
} Header;

// Plymorphism in C

typedef struct {
  int kind; // 0 = camera, 1 = sphere, 2 = plane
  double color[3];
  union {
    struct {
      double width;
      double height;
    } camera;
    struct {
      double position[3];
      double radius;
    } sphere;
    struct {
      double position[3];
      double normal[3];
    } plane;
  };
} Object;

double sphere_intersection(double*, double*, double*, double);
double plane_intersection(double*, double*, double*, double*);
void writeP3(Pixel *, Header, FILE *);
int next_c(FILE*);
void expect_c(FILE*, int);
void skip_ws(FILE*);
char* next_string(FILE*);
double next_number(FILE*);
double* next_vector(FILE*);
Object** read_scene(char*);

static inline double sqr(double v) {
  return v*v;
}


static inline void normalize(double* v) {
  double len = sqrt(sqr(v[0]) + sqr(v[1]) + sqr(v[2]));
  v[0] /= len;
  v[1] /= len;
  v[2] /= len;
}

int main(int argc, char *argv[]) {
  if (argc != 5) {
    fprintf(stderr, "Error: Incorrect number of arguments.\n");
    printf("Usage: raycast width height input.json output.ppm\n");
    return(1);
  }
  
  // Get dimensions
  int N = atoi(argv[1]);
  int M = atoi(argv[2]);
  
  if (N <= 0 || M <= 0) {
    fprintf(stderr, "Error: Invalid dimensions.\n");
    exit(1);
  }
  
  // Read the scene file
  Object** objects = read_scene(argv[3]);
  
  double cx = 0;
  double cy = 0;
  
  double h;
  double w;
  
  // Find the camera and get the height and width
  int cur = 0;
  int cam = 0;
  while (objects[cur] != NULL) {
    if (objects[cur]->kind == 0) {
      h = objects[cur]->camera.height;
      w = objects[cur]->camera.width;
      cam += 1;
    }
    cur += 1;
  }
  
  if (cam = 0) {
    fprintf(stderr, "Error: No camera found.\n");
    exit(1);
  }
  
  if (cam > 1) {
    fprintf(stderr, "Error: Multiple cameras not supported.\n");
    exit(1);
  }
  
  // Initialize pixel buffer
  Pixel *buffer = malloc(sizeof(Pixel) * N * M);
  
  double pixheight = h / M;
  double pixwidth = w / N;
  for (int y = M; y > 0; y -= 1) { // Going through y from greatest to least
                                   // for convenient pixel output
    for (int x = 0; x < N; x += 1) {
      // Raycast
      
      double Ro[3] = {0, 0, 0};
      // Rd = normalize(P - Ro)
      double Rd[3] = {
        cx - (w/2) + pixwidth * (x + 0.5),
        cy - (h/2) + pixheight * (y + 0.5),
        1
      };
      normalize(Rd);

      double best_t = INFINITY;
      int best_i = 0;
      // Check intersections
      for (int i=0; objects[i] != 0; i += 1) {
        double t = 0;
        // Call correct intersection function
        switch(objects[i]->kind) {
        case 0:
          t = -1;
          break;
        case 1:
          t = sphere_intersection(Ro, Rd,
                                    objects[i]->sphere.position,
                                    objects[i]->sphere.radius);
          break;
        case 2:
          t = plane_intersection(Ro, Rd,
                                    objects[i]->plane.position,
                                    objects[i]->plane.normal);
          break;
        default:
          fprintf(stderr, "Error: Programmer forgot to implement an intersection.");
          exit(1);
        }
        if (t > 0 && t < best_t) {
          best_t = t;
          best_i = i;
        }
      }
      // Note: Going through y in reverse, so adjust index accordingly
      int p = (M - y)*N + x; // Index of buffer
      if (best_t > 0 && best_t != INFINITY) {
        // Scale color values for ppm output
        buffer[p].red = (char) (objects[best_i]->color[0] * 255);
        buffer[p].green = (char) (objects[best_i]->color[1] * 255);
        buffer[p].blue = (char) (objects[best_i]->color[2] * 255);
      } else {
        // If there is no intersection, have a black background
        buffer[p].red = 0;
        buffer[p].green = 0;
        buffer[p].blue = 0;
      }
      
    }
  }
  
  FILE* output = fopen(argv[4], "w");
  
  if (output == NULL) {
    fprintf(stderr, "Error: Could not write to file \"%s\"\n", argv[4]);
    exit(1);
  }
  
  Header outHeader;
  outHeader.magicNumber = 3;
  outHeader.maxColor = 255;
  outHeader.width = N;
  outHeader.height = M;
  
  writeP3(buffer, outHeader, output);
  
  return 0;
}

double sphere_intersection(double* Ro, double* Rd,
                             double* C, double r) {
  double a = (sqr(Rd[0]) + sqr(Rd[1]) + sqr(Rd[2]));
  double b = 2 * (Rd[0] * (Ro[0] - C[0]) + Rd[1] * (Ro[1] - C[1]) + Rd[2] * (Ro[2] - C[2]));  
  double c = sqr(Ro[0] - C[0]) + sqr(Ro[1] - C[1]) + sqr(Ro[2] - C[2]) - sqr(r);

  double det = sqr(b) - 4 * a * c;
  if (det < 0) return -1;

  det = sqrt(det);
  
  double t0 = (-b - det) / (2*a);
  if (t0 > 0) return t0;

  double t1 = (-b + det) / (2*a);
  if (t1 > 0) return t1;

  return -1;
}

double plane_intersection(double* Ro, double* Rd,
                             double* P, double* n) {
  double t = -(n[0]*(Ro[0]-P[0]) + n[1]*(Ro[1]-P[1]) + n[2]*(Ro[2]-P[2])) / (n[0]*Rd[0] + n[1]*Rd[1] + n[2]*Rd[2]);
  return t;
}


// Writes P3 data
void writeP3(Pixel *buffer, Header h, FILE *fh) {
  // Write the header
  fprintf(fh, "P%d\n%d %d\n%d\n", h.magicNumber, h.width, h.height, h.maxColor);
  // Write the ascii data
  for (int i = 0; i < h.width * h.height; i++) {
     fprintf(fh, "%d\n%d\n%d\n", buffer[i].red, buffer[i].green, buffer[i].blue);
  }
}

// next_c() wraps the getc() function and provides error checking and line
// number maintenance
int next_c(FILE* json) {
  int c = fgetc(json);
#ifdef DEBUG
  printf("next_c: '%c'\n", c);
#endif
  if (c == '\n') {
    line += 1;
  }
  if (c == EOF) {
    fprintf(stderr, "Error: Unexpected end of file on line number %d.\n", line);
    exit(1);
  }
  return c;
}


// expect_c() checks that the next character is d.  If it is not it emits
// an error.
void expect_c(FILE* json, int d) {
  int c = next_c(json);
  if (c == d) return;
  fprintf(stderr, "Error: Expected '%c' on line %d.\n", d, line);
  exit(1);    
}


// skip_ws() skips white space in the file.
void skip_ws(FILE* json) {
  int c = next_c(json);
  while (isspace(c)) {
    c = next_c(json);
  }
  ungetc(c, json);
}


// next_string() gets the next string from the file handle and emits an error
// if a string can not be obtained.
char* next_string(FILE* json) {
  char buffer[129];
  int c = next_c(json);
  if (c != '"') {
    fprintf(stderr, "Error: Expected string on line %d.\n", line);
    exit(1);
  }  
  c = next_c(json);
  int i = 0;
  while (c != '"') {
    if (i >= 128) {
      fprintf(stderr, "Error: Strings longer than 128 characters in length are not supported.\n");
      exit(1);      
    }
    if (c == '\\') {
      fprintf(stderr, "Error: Strings with escape codes are not supported.\n");
      exit(1);      
    }
    if (c < 32 || c > 126) {
      fprintf(stderr, "Error: Strings may contain only ascii characters.\n");
      exit(1);
    }
    buffer[i] = c;
    i += 1;
    c = next_c(json);
  }
  buffer[i] = 0;
  return strdup(buffer);
}

double next_number(FILE* json) {
  double value;
  int cnt = fscanf(json, "%lf", &value);
  if (cnt != 1) {
    fprintf(stderr, "Error: Could not read number on line %d.\n", line);
    exit(1);
  }
  return value;
}

double* next_vector(FILE* json) {
  double* v = malloc(3*sizeof(double));
  expect_c(json, '[');
  skip_ws(json);
  v[0] = next_number(json);
  skip_ws(json);
  expect_c(json, ',');
  skip_ws(json);
  v[1] = next_number(json);
  skip_ws(json);
  expect_c(json, ',');
  skip_ws(json);
  v[2] = next_number(json);
  skip_ws(json);
  expect_c(json, ']');
  return v;
}


Object** read_scene(char* filename) {
  int c;
  
  Object** objects;
  objects = malloc(sizeof(Object*)*129);
  
  FILE* json = fopen(filename, "r");

  if (json == NULL) {
    fprintf(stderr, "Error: Could not open file \"%s\"\n", filename);
    exit(1);
  }
  
  skip_ws(json);
  
  // Find the beginning of the list
  expect_c(json, '[');

  skip_ws(json);

  // Find the objects
  int objcnt = 0;
  while (1) {
    c = fgetc(json);
    if (c == ']') {
      fprintf(stderr, "Error: This is the worst scene file EVER.\n");
      fclose(json);
      exit(1);
    }
    if (c == '{') {
      skip_ws(json);
    
      // Parse the object
      char* key = next_string(json);
      if (strcmp(key, "type") != 0) {
        fprintf(stderr, "Error: Expected \"type\" key on line number %d.\n", line);
        exit(1);
      }

      skip_ws(json);

      expect_c(json, ':');

      skip_ws(json);

      char* value = next_string(json);
      if (strcmp(value, "camera") == 0) {     
        objects[objcnt] = malloc(sizeof(Object));
        objects[objcnt]->kind = 0;
      } else if (strcmp(value, "sphere") == 0) {
        objects[objcnt] = malloc(sizeof(Object));
        objects[objcnt]->kind = 1;
      } else if (strcmp(value, "plane") == 0) {
        objects[objcnt] = malloc(sizeof(Object));
        objects[objcnt]->kind = 2;
      } else {
        fprintf(stderr, "Error: Unknown type, \"%s\", on line number %d.\n", value, line);
        exit(1);
      }

      skip_ws(json);

      int valcnt = 0;
      while (1) {
        // , }
        c = next_c(json);
        if (c == '}') {
          // stop parsing this object
          switch (objects[objcnt]->kind) {
          case 0:
            if (valcnt != 2) {
              fprintf(stderr, "Error: Bad value count.");
              exit(1);
            }
            break;
          case 1:
            if (valcnt != 3) {
              fprintf(stderr, "Error: Bad value count.");
              exit(1);
            }
            break;
          case 2:
            if (valcnt != 3) {
              fprintf(stderr, "Error: Bad value count.");
              exit(1);
            }
            break;
          default:
            fprintf(stderr, "Error: Unexpected key on line %d.\n", line);
            exit(1);
            break;
          }
          
          
          objcnt += 1;
          if (objcnt > 128) {
            fprintf(stderr, "Error: 128 object count exceeded.");
            exit(1);
          }
          break;
        } else if (c == ',') {
          // read another field
          skip_ws(json);
          char* key = next_string(json);
          skip_ws(json);
          expect_c(json, ':');
          skip_ws(json);
          
          
          
          if (strcmp(key, "width") == 0) {
            double value = next_number(json);
            switch (objects[objcnt]->kind) {
            case 0:
              objects[objcnt]->camera.width = value;
              break;
            default:
              fprintf(stderr, "Error: Unexpected key on line %d.\n", line);
              exit(1);
              break;
            }
          } else if (strcmp(key, "height") == 0) {
            double value = next_number(json);
            switch (objects[objcnt]->kind) {
            case 0:
              objects[objcnt]->camera.height = value;
              break;
            default:
              fprintf(stderr, "Error: Unexpected key on line %d.\n", line);
              exit(1);
              break;
            }
          } else if (strcmp(key, "radius") == 0) {
            double value = next_number(json);
            switch (objects[objcnt]->kind) {
            case 1:
              objects[objcnt]->sphere.radius = value;
              break;
            default:
              fprintf(stderr, "Error: Unexpected key on line %d.\n", line);
              exit(1);
              break;
            }
          } else if (strcmp(key, "color") == 0) {
            double* value = next_vector(json);
            switch (objects[objcnt]->kind) {
            case 0:
              fprintf(stderr, "Error: Unexpected key on line %d.\n", line);
              exit(1);
              break;
            default:
              objects[objcnt]->color[0] = value[0];
              objects[objcnt]->color[1] = value[1];
              objects[objcnt]->color[2] = value[2];
              break;
            }
          } else if (strcmp(key, "position") == 0){
            double* value = next_vector(json);
            switch (objects[objcnt]->kind) {
            case 1:
              objects[objcnt]->sphere.position[0] = value[0];
              objects[objcnt]->sphere.position[1] = value[1];
              objects[objcnt]->sphere.position[2] = value[2];
              break;
            case 2:
              objects[objcnt]->plane.position[0] = value[0];
              objects[objcnt]->plane.position[1] = value[1];
              objects[objcnt]->plane.position[2] = value[2];
              break;
            default:
              fprintf(stderr, "Error: Unexpected key on line %d.\n", line);
              exit(1);
              break;
            }
          } else if (strcmp(key, "normal") == 0) {
            double* value = next_vector(json);
            switch (objects[objcnt]->kind) {
            case 2:
              objects[objcnt]->plane.normal[0] = value[0];
              objects[objcnt]->plane.normal[1] = value[1];
              objects[objcnt]->plane.normal[2] = value[2];
              break;
            default:
              fprintf(stderr, "Error: Unexpected key on line %d.\n", line);
              exit(1);
              break;
            }
          } else {
            fprintf(stderr, "Error: Unknown property, \"%s\", on line %d.\n",
                    key, line);
            //char* value = next_string(json);
          }
          valcnt += 1;
          skip_ws(json);
        } else {
          fprintf(stderr, "Error: Unexpected value on line %d\n", line);
          exit(1);
        }
      }
      skip_ws(json);
      c = next_c(json);
      if (c == ',') {
        // noop
        skip_ws(json);
      } else if (c == ']') {
        objects[objcnt] = NULL;
        fclose(json);
        return objects;
      } else {
        fprintf(stderr, "Error: Expecting ',' or ']' on line %d.\n", line);
        exit(1);
      }
    }

  }

  
  return NULL;
}

