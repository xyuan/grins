#!/bin/bash

PROG="${GRINS_TEST_DIR}/generic_solution_regression"

INPUT="${GRINS_TEST_INPUT_DIR}/thermally_driven_2d_flow.in"
DATA="${GRINS_TEST_DATA_DIR}/thermally_driven_2d.xdr"

PETSC_OPTIONS="-pc_type asm -pc_asm_overlap 10 -sub_pc_type ilu -sub_pc_factor_levels 10"

${LIBMESH_RUN:-} $PROG $PETSC_OPTIONS input=$INPUT soln-data=$DATA vars='u v p T' norms='L2 H1' tol='8.0e-9'
