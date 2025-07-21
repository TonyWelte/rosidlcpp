^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Changelog for package rosidlcpp_generator_c
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

0.3.0 (2025-07-21)
------------------
* Add ros_environment dependency to fix missing ROS_DISTRO variable (`#13 <https://github.com/TonyWelte/rosidlcpp/issues/13>`_)
* Fix compilation on RHEL9 (`#12 <https://github.com/TonyWelte/rosidlcpp/issues/12>`_)
* Add jazzy support (`#11 <https://github.com/TonyWelte/rosidlcpp/issues/11>`_)
* Contributors: Anthony Welte

0.2.1 (2025-07-14)
------------------
* Fix CRLF interface files (`#10 <https://github.com/TonyWelte/rosidlcpp/issues/10>`_)
* Contributors: Anthony Welte

0.2.0 (2025-06-24)
------------------
* Fix shared/static library type for generator_c and introspection_c[pp] (`#6 <https://github.com/TonyWelte/rosidlcpp/issues/6>`_)
* Contributors: Anthony Welte

0.1.2 (2025-06-21)
------------------

0.1.1 (2025-06-15)
------------------
* Fix circular dependency caused by ament_cmake_ros dependency (`#4 <https://github.com/TonyWelte/rosidlcpp/issues/4>`_)
* Contributors: Anthony Welte

0.1.0 (2025-05-25)
------------------
* Fix escaped strings to match changes from https://github.com/ros2/rosidl/pull/862
* First release of rosidlcpp_generator_c package
* Contributors: Anthony Welte
