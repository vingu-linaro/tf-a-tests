#
# Copyright (c) 2018-2022, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

# Default number of threads per CPU on FVP
FVP_MAX_PE_PER_CPU		:= 1

# Check the PE per core count
ifneq ($(FVP_MAX_PE_PER_CPU),$(filter $(FVP_MAX_PE_PER_CPU),1 2))
$(error "Incorrect FVP_MAX_PE_PER_CPU = ${FVP_MAX_PE_PER_CPU} \
	specified for FVP port")
endif

# Default cluster count and number of CPUs per cluster for FVP
ifeq ($(FVP_MAX_PE_PER_CPU),1)
FVP_CLUSTER_COUNT		:= 2
FVP_MAX_CPUS_PER_CLUSTER	:= 4
else
FVP_CLUSTER_COUNT		:= 1
FVP_MAX_CPUS_PER_CLUSTER	:= 8
endif

# Check cluster count and number of CPUs per cluster
ifeq ($(FVP_MAX_PE_PER_CPU),2)
# Multithreaded CPU: 1 cluster with up to 8 CPUs
$(eval $(call CREATE_SEQ,CLS,1))
$(eval $(call CREATE_SEQ,CPU,8))
else
# CPU inside DynamIQ Shared Unit: 1 cluster with up to 8 CPUs
ifeq ($(FVP_CLUSTER_COUNT),1)
$(eval $(call CREATE_SEQ,CLS,1))
$(eval $(call CREATE_SEQ,CPU,8))
else
# CPU with single thread: max 4 clusters with up to 4 CPUs
$(eval $(call CREATE_SEQ,CLS,4))
$(eval $(call CREATE_SEQ,CPU,4))
endif
endif

# Check cluster count
ifneq ($(FVP_CLUSTER_COUNT),$(filter $(FVP_CLUSTER_COUNT),$(CLS)))
  $(error "Incorrect FVP_CLUSTER_COUNT = ${FVP_CLUSTER_COUNT} \
  specified for FVP port with \
  FVP_MAX_CPUS_PER_CLUSTER = ${FVP_MAX_CPUS_PER_CLUSTER} \
  FVP_MAX_PE_PER_CPU = ${FVP_MAX_PE_PER_CPU}")
endif

# Check number of CPUs per cluster
ifneq ($(FVP_MAX_CPUS_PER_CLUSTER),$(filter $(FVP_MAX_CPUS_PER_CLUSTER),$(CPU)))
  $(error "Incorrect FVP_MAX_CPUS_PER_CLUSTER = ${FVP_MAX_CPUS_PER_CLUSTER} \
  specified for FVP port with \
  FVP_CLUSTER_COUNT = ${FVP_CLUSTER_COUNT} \
  FVP_MAX_PE_PER_CPU = ${FVP_MAX_PE_PER_CPU}")
endif

# Pass FVP topology definitions to the build system
$(eval $(call add_define,SCMI_DEFINES,FVP_CLUSTER_COUNT))
$(eval $(call add_define,SCMI_DEFINES,FVP_MAX_CPUS_PER_CLUSTER))
$(eval $(call add_define,SCMI_DEFINES,FVP_MAX_PE_PER_CPU))

# Default PA size for FVP platform
PA_SIZE := 36

$(eval $(call add_define,SCMI_DEFINES,PA_SIZE))

