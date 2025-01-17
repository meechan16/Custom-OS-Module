#include "loader.h"

Elf32_Ehdr *ehdr;
Elf32_Phdr *phdr;
int fd;
void loader_cleanup() {
  if (ehdr != NULL) {
      free(ehdr);
  }
  if (phdr != NULL) {
    free(phdr);
  }

  if (fd >= 0) {
    close(fd);
    fd = -1;
  }
}

void load_and_run_elf(char** exe) {
  
  fd = open(exe[1], O_RDONLY); //Open the ELF file
  if (fd == -1) {
    printf("Can't read file");  //Error handling if file not open or missing
    exit(1);
  }

  ehdr = malloc(sizeof(Elf32_Ehdr)); // memory assignment to the EHDR
  if (ehdr == NULL) {     
    printf("Can't allocate Memory");
    exit(1);
  }
  
  if (read(fd,ehdr,sizeof(Elf32_Ehdr)) != sizeof(Elf32_Ehdr)) { // reading the ehdr using the read function
    printf("Can't read Elf Header");
    exit(1);
  };



  phdr = malloc(sizeof(Elf32_Phdr)); // assigning memory for program headers
  
  if(phdr == NULL) { 
    printf("Can't Allocate Memory");
    exit(1);
  }
  
  if(lseek(fd,ehdr->e_phoff,SEEK_SET) == -1) {     // from where the array of segments start offset is given by ehdr->e_phoff 
    printf("Can't find Offset");
    exit(1);
  };

  for (int i = 0; i < ehdr->e_phnum; i++) {

    if(read(fd,phdr,sizeof(Elf32_Phdr)) != sizeof(Elf32_Phdr)) { // using the read function to read the phdr
      printf("Can't load Program Header");
      exit(1);
    };

    if (phdr->p_type == PT_LOAD && ehdr->e_entry <= phdr->p_vaddr + phdr->p_memsz && ehdr->e_entry >= phdr->p_vaddr) { // checking for PT_LOAD
        break;
    }

  }
  
  void * _virtual_mem = mmap(NULL, phdr->p_memsz, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_ANONYMOUS | MAP_PRIVATE, 0, 0); //Allocating memory of the size “p_memsz” using mmap function
  if (lseek(fd,phdr->p_offset,SEEK_SET) == -1) { //
    printf("Can't find offeset");
    exit(1);
  };

  if (read(fd,_virtual_mem,phdr->p_memsz) != phdr->p_memsz) { // using the read function to read the virtual memory
    printf("Cannot read virtual memory");
    exit(1);
  };

  
  unsigned int relative_distance = ehdr->e_entry - phdr->p_vaddr; //Navigating to the entrypoint address into the segment loaded in the memory by calculating its address.
  void * real_entry = relative_distance + _virtual_mem;
  
  int(*_start)() = (int(*)())real_entry;  //Typecasting the address to that of function pointer matching "_start" method in fib.c.
  
  
  int result = _start(); //Calling the "_start" method
  printf("User _start return value = %d\n",result); //Printing the result
}

int main(int argc, char** argv) 
{
  if(argc != 2) {
    printf("Usage: %s <ELF Executable> \n",argv[0]);
    exit(1);
  }
  //cheching has been done in load_And_run_elf function();
  // 2. passing it to the loader for carrying out the loading/execution
  load_and_run_elf(argv);
  // 3. invoke the cleanup routine inside the loader  
  loader_cleanup();
  return 0;
}

