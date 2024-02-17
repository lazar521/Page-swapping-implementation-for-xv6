# Overview

This project was done as a part of my operating systems course.
<br>

The goal was to implement page swapping capabilities into the xv6 kernel. The technique I used is LRU (least recently used). 

The whole implementation of the page swapping functionality is in pgswapper.c and the vm.c and trap.c files just use the interface defined in pgswapper.h. 
<br>


## Testing
The project was built using a Ubuntu VM provided by the University. The Ubuntu VM can be found [here](https://drive.google.com/file/d/1edGYFcvdnV0pbKws_1G1vePtEec0qC0G/view). The password for the VM is 'os'


## Disclaimer

Aside from:
- kernel/pgswapper.c  
- kernel/pgswapper.h  
<br>

and a few minor modifications in:
- kernel/vm.c  
- kernel/trap.c
<br>

everything else was provided by the University.

 

