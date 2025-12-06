- The project is layered (upper to lower):

    - global

    - function

    - resource

    - core

    - platform

- Only upper layer depending lower layer is allowed. Lower layer depending on upper layer is
prohibited.

- Template use in layers above function layer (including function layer) is prohibited.
