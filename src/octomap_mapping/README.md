This octomap_server package has been copied to a new repository and will thus not see any more development.
Please find the original package at https://github.com/OctoMap/octomap_mapping.git

I found information that helped me add the package here https://github.com/OctoMap/octomap_mapping/issues/30
The last time this worked properly was in indigo verions.
This current branch for kinetic is in development stage.

Introduction:
This package is slightly modified and allows you to save and load
octomap files (bt and ot).




Requirements/Installation:

- A computer to run the code from
- Linux operating system (either real or virtual)
- ROS Kinetic installed on your computer (1)
- Turtlebot installed on your computer (2)
- This 'octomap_server' package should be in your workspace

(1)http://wiki.ros.org/kinetic/Installation/Ubuntu
(2)https://answers.ros.org/question/246015/installing-turtlebot-on-ros-kinetic/




Usage:

When everything is set up as described in the above steps,
perform the following steps to use the package:
- roslaunch octomap_server octomapping_in_simulation.launch

Gazebo simulation will open to show the simulated world.
Rviz will open to show you the 3d map that is being created
of the simulated world.
A new terminal has opened where you can teleoperate the turtlebot
using your keyboard.


Maintainers:

Aron Haraldsson - ronnamura@gmail.com



FAQ/Troubleshooting:


