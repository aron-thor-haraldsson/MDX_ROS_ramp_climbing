This octomap_server package has been copied to a new repository and will thus not see any more development.
Please find the original package at https://github.com/OctoMap/octomap_mapping.git

I found information that helped me add the package here https://github.com/OctoMap/octomap_mapping/issues/30
The last time this worked properly was in indigo verions.
This current branch for kinetic is in development stage.

Introduction:

This is a package that connects to a Baxter robot and performs a pick and place routine.
The code uses image processing and machine learning to recognise shapes
that the Baxter arm then picks up and relocates.




Requirements/Installation:

- a computer to run the code from
- Linux operating system (either real or virtual)
- ROS Kinetic installed on your computer (1)
- Turtlebot installed on your computer (2)
- the 'octomap_server' package should be in your workspace

(1)http://wiki.ros.org/kinetic/Installation/Ubuntu
(2)https://answers.ros.org/question/246015/installing-turtlebot-on-ros-kinetic/




Usage:

When everything is set up as described in the above steps,
perform the following steps to use the package:
- roslaunch turtlebot_gazebo turtlebot_world.launch
- roslaunch turtlebot_gazebo gmapping_demo.launch
- roslaunch octomap_server octomap
- rosrun 'tuck.py -u' from the package 'baxter_tools'
- roslaunch the 'baxter_pick_place.launch' from the package 'baxter_pick_place_pkg'
- enjoy


Maintainers:

Aron Haraldsson - ronnamura@gmail.com



FAQ/Troubleshooting:


