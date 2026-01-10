#include "kalloc.h"
#include "x86.h"
#include <vm.h>

pde *kpgdir;

// void kvmalloc() {
//     kpgdir = setupkvm();
//     switchkvm();
// }
//
// void setupkvm
//
// void switchkvm() {
//     lcr3((uint)kpgdir);
// }
