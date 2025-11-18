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

Frame
-----

The Frame API provides access to video frames.  They can be used free-standing
for cases where the user requires access to the Frame API without the need for
the sharing mechanism.  When sharing is required then frames must be accessed
through the Client API or published through the Host API.

.. doxygenstruct:: VSLFrame

.. doxygenfunction:: vsl_frame_init
.. doxygenfunction:: vsl_frame_release

Client
------

The Client API allows the user to receive frames published by the Host API.

.. doxygenstruct:: VSLClient

.. doxygenfunction:: vsl_client_init
.. doxygenfunction:: vsl_client_release

Host
----

The Host API allows the user to publish frames to be retreived by the Client
API.

.. doxygenstruct:: VSLHost

.. doxygenfunction:: vsl_host_init
.. doxygenfunction:: vsl_host_release

Encoder
-------

The Encoder API provides access to the platform's video codec to encode video
as H.264 or H.265.

.. doxygenstruct:: VSLEncoder


Versions
~~~~~~~~

.. doxygendefine:: VSL_TARGET_VERSION
.. doxygendefine:: VSL_VERSION_1_0
.. doxygendefine:: VSL_VERSION_1_1
.. doxygendefine:: VSL_VERSION_1_2
.. doxygendefine:: VSL_VERSION_1_3
