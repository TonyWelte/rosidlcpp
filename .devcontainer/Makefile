.PHONY: clean_rosidl clean_rosidlpp clean_rosidlcpp_no_precompiled clean_all build_rosidl build_rosidlcpp build_rosidlcpp_no_precompiled build_all

clean_rosidl:
	@rm -rf build_rosidl/${PKG}

clean_rosidlcpp:
	@rm -rf build_rosidlcpp/${PKG}

clean_rosidlcpp_no_precompiled:
	@rm -rf build_rosidlcpp_no_precompiled/${PKG}

clean_all:
	@$(MAKE) -f $(lastword $(MAKEFILE_LIST)) clean_rosidl
	@$(MAKE) -f $(lastword $(MAKEFILE_LIST)) clean_rosidlcpp
	@$(MAKE) -f $(lastword $(MAKEFILE_LIST)) clean_rosidlcpp_no_precompiled

build_rosidl:
	@env -i bash -c "source /opt/ros/${ROS_DISTRO}/setup.bash; colcon build --mixin ninja --packages-select ${PKG} --base-paths src --build-base build_rosidl --install-base install_rosidl --event-handlers=console_direct+"

build_rosidlcpp:
	@env -i bash -c "source /opt/ros/${ROS_DISTRO}/setup.bash; source install/setup.bash; colcon build --mixin ninja --packages-select ${PKG} --base-paths src --build-base build_rosidlcpp --install-base install_rosidlcpp --event-handlers=console_direct+"

build_rosidlcpp_no_precompiled:
	@env -i bash -c "source /opt/ros/${ROS_DISTRO}/setup.bash; source install/setup.bash; colcon build --mixin ninja --packages-select ${PKG} --base-paths src --build-base build_rosidlcpp_no_precompiled --install-base install_rosidlcpp_no_precompiled --cmake-args -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON"

build_all:
	@$(MAKE) -f $(lastword $(MAKEFILE_LIST)) build_rosidl
	@$(MAKE) -f $(lastword $(MAKEFILE_LIST)) build_rosidlcpp
	@$(MAKE) -f $(lastword $(MAKEFILE_LIST)) build_rosidlcpp_no_precompiled

diff:
	@echo "----------------------------------------------------------------------------"
	@echo "Comparing ${PKG} for ${GENERATOR}"
	@echo "----------------------------------------------------------------------------"
	@diff --color -r -N build_rosidl/${PKG}/${GENERATOR}  build_rosidlcpp/${PKG}/${GENERATOR} || true

diff_all:
	@$(MAKE) -f $(lastword $(MAKEFILE_LIST)) GENERATOR=rosidl_generator_c diff
	@$(MAKE) -f $(lastword $(MAKEFILE_LIST)) GENERATOR=rosidl_generator_cpp diff
	@$(MAKE) -f $(lastword $(MAKEFILE_LIST)) GENERATOR=rosidl_generator_py diff
	@$(MAKE) -f $(lastword $(MAKEFILE_LIST)) GENERATOR=rosidl_generator_cpp diff
	@$(MAKE) -f $(lastword $(MAKEFILE_LIST)) GENERATOR=rosidl_generator_type_description diff
	@$(MAKE) -f $(lastword $(MAKEFILE_LIST)) GENERATOR=rosidl_typesupport_c diff
	@$(MAKE) -f $(lastword $(MAKEFILE_LIST)) GENERATOR=rosidl_typesupport_cpp diff
	@$(MAKE) -f $(lastword $(MAKEFILE_LIST)) GENERATOR=rosidl_typesupport_fastrtps_c diff
	@$(MAKE) -f $(lastword $(MAKEFILE_LIST)) GENERATOR=rosidl_typesupport_fastrtps_cpp diff
	@$(MAKE) -f $(lastword $(MAKEFILE_LIST)) GENERATOR=rosidl_typesupport_introspection_c diff
	@$(MAKE) -f $(lastword $(MAKEFILE_LIST)) GENERATOR=rosidl_typesupport_introspection_cpp diff