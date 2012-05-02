-=-=-=-=-
polysplit
-=-=-=-=-

Polysplit reads a polygon or multipolygon layer from an OGR datasource, and
breaks each component polygon into pieces, such that the union of the pieces is
equal to the original (multi)polygon, but none of the components have more than
n vertices.

Each feature in the output layer has a single attribute, consisting of an
integer identifier copied from the input layer. If no ID field is specified,
the OGR feature ID is used. For Shapefiles, this is just the ordinal index of
the feature. The ID field, if specified, must be of integer type.

The reason you'd want to do such a thing is to be able to do fast
point-in-polygon queries, while still being able to refer to the original
(multi)polygon feature.

-----
Usage
-----

polysplit [opts] <input datasource> <output datasource>

    -i    input layer name
    -o    output layer name
    -f    OGR output format
    -n    ID field name (must be integer type)
    -m    Max vertices per output polygon (defaults to 250)
    -v    Verbose mode

-------------
Prerequisites
-------------

* GNU g++. Will probably work with other C++ compilers.
* make
* GEOS
* GDAL, compiled with support for OGR and GEOS. http://gdal.org/

On Debian or Ubuntu, run:

  apt-get install build-essential libgdal1-dev 

-----------
Compilation
-----------

Run `make`. There's no `make install`, sorry. :)

-------
Credits
-------

Polysplit was written by Schuyler Erle <schuyler@nocat.net>, with the support
of Geoloqi.

-------
License
-------

Copyright (c) 2012 Geoloqi, Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

  * Redistributions of source code must retain the above copyright notice, this
    list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

  * Neither the name of Geoloqi nor the names of its contributors
    may be used to endorse or promote products derived from this software
    without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Blah blah blah.

-End-
