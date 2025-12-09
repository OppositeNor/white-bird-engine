Tick render stages:

- pre-execution tasks: A serial of callback functions, with a command buffer as parameter,
which the callback functions could record commands in the command buffer, and it would be
pushed to the command queue before all the tasks are executing in the next frame.

- Run render tasks: As how they are passed into the render task compiler, in the order of
BFS, run each tasks. A task will only be run after the dependent task finishes (implemented
based on semaphores).
