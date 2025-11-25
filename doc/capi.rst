C API Reference
===============

The VideoStream C API is modelled around opaque structures, referred to as
classes, from which objects are created and interfaced through the various
access functions.  The internals of the structures are not exposed, instead the
access functions are documented and carry the pointer to the opaque object.

Base Functions
--------------

.. doxygenfunction:: vsl_version
.. doxygenfunction:: vsl_timestamp
.. doxygenfunction:: vsl_fourcc_from_string

Types and Structures
--------------------

.. doxygenstruct:: VSLHost
   :members:

.. doxygenstruct:: VSLClient
   :members:

.. doxygenstruct:: VSLFrame
   :members:

.. doxygenstruct:: VSLEncoder
   :members:

.. doxygenstruct:: VSLDecoder
   :members:

.. doxygenstruct:: VSLCamera
   :members:

.. doxygenstruct:: VSLRect
   :members:

.. doxygentypedef:: vsl_frame_cleanup

Enumerations
~~~~~~~~~~~~

.. doxygenenum:: VSLEncoderProfile
.. doxygenenum:: VSLDecoderRetCode

Version Constants
-----------------

.. doxygendefine:: VSL_VERSION
.. doxygendefine:: VSL_TARGET_VERSION
.. doxygendefine:: VSL_VERSION_1_0
.. doxygendefine:: VSL_VERSION_1_1
.. doxygendefine:: VSL_VERSION_1_2
.. doxygendefine:: VSL_VERSION_1_3
.. doxygendefine:: VSL_VERSION_1_4

Macros
------

.. doxygendefine:: VSL_FOURCC
