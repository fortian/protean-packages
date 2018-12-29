RAPR Version 1.0.0

This directory contains source code for the NRL Real-Time Application
Representative (RAPR) Test Tool Version 1.0.0

RAPR can be used to generate and respond to network traffic in order
to emulate generic application behaviors, or "representative
applications".  It uses the Mgen toolkit to generate network traffic
and supports most of the traffic shaping functionality available in
Mgen Version 5.01b (see http://pf.itd.nrl.navy.mil/mgen/mgen.html for
more details)

Rapr behavior is defined by a RAPR input script, a behavior table (or
logictable), and RAPR commands received over a run time interface.
The RAPR input script is used to schedule emulated application
behavior over a time line.  The run time interface can be used to
inject and schedule additional RAPR behaviors.  The behavior table
defines behavior in response to network or internally generated
events.  Additionally a dictionary is used to define name-value pairs
used by the application.


FILES AND DIRECTORIES:

README.TXT   - this file.

common       - Directory containing RAPR source code files

docs         - rapr user's guide (rapr.html) and various readme files

docs/samples - sample rapr scripts

unix         - Directory with Unix-specific source and Makefiles

Required Distributions:

mgen-5.01b - NRL MGEN source tree (See
             <http://cs.itd.nrl.navy.mil/products>)

protolib   - NRL Protolib source tree (See
             <http://cs.itd.nrl.navy.mil/products>)

sprng 2.0  - <http://sprng.cs.fsu.edu/>

stlport    - <http://www.stlport.org/>
4.6.2+

Quick Start Installation:

The RAPR source code expects the "mgen" directory on the same level as
the "rapr" root directory.  Modify the /rapr/unix/Makefile.common
PROTOLIB, MGEN_COMMON, and MGEN_BUILD directories to change this
location.

If the STLport libraries are not in a system wide location, modify the
Makefile.common STLPORT_INC and STLPORT_LIB directories to point the
the correct locations.

Modify the /rapr/unix/Makefile.common SPRNG_DIR, SPRNG_INC, and
SPRNG_LIB directories to point to the location of the sprng lib and
include directories.

If you choose to build the sprng library withouth MPI or pmlcg
(neither of which are required for use with RAPR) you may need to make
the following changes to sprng:

Modify the make.CHOICES file to exlude MPIDEF, PMLCGDEF, and GMPLIB.

Modify the SRC/make.INTEL file to not include the mpi include
directories.

Remove references to pmlcg from the SRC/sprng/Makefile.  Modify the
GENLIST and libsprng.a src list.

Feel free to email with questions and comments.

Brian Adamson <adamson@itd.nrl.navy.mil>

