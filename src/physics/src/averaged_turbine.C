//-----------------------------------------------------------------------bl-
//--------------------------------------------------------------------------
// 
// GRINS - General Reacting Incompressible Navier-Stokes 
//
// Copyright (C) 2014 Paul T. Bauman, Roy H. Stogner
// Copyright (C) 2010-2013 The PECOS Development Team
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the Version 2.1 GNU Lesser General
// Public License as published by the Free Software Foundation.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc. 51 Franklin Street, Fifth Floor,
// Boston, MA  02110-1301  USA
//
//-----------------------------------------------------------------------el-


// This class
#include "grins/averaged_turbine.h"

// GRINS
#include "grins/generic_ic_handler.h"
#include "grins/variable_name_defaults.h"

// libMesh
#include "libmesh/quadrature.h"
#include "libmesh/boundary_info.h"
#include "libmesh/parsed_function.h"
#include "libmesh/zero_function.h"

namespace GRINS
{

  AveragedTurbine::AveragedTurbine( const std::string& physics_name, const GetPot& input )
    : IncompressibleNavierStokesBase(physics_name, input)
  {
    this->read_input_options(input);

    this->_ic_handler = new GenericICHandler( physics_name, input );

    return;
  }

  AveragedTurbine::~AveragedTurbine()
  {
    return;
  }

  void AveragedTurbine::init_variables( libMesh::FEMSystem* system )
  {
    this->_fan_speed_var = system->add_variable(_fan_speed_var_name,
                                                libMesh::FIRST,
                                                libMesh::SCALAR);

    IncompressibleNavierStokesBase::init_variables(system);
  }

  void AveragedTurbine::set_time_evolving_vars( libMesh::FEMSystem* system )
  {
    system->time_evolving(this->fan_speed_var());

    IncompressibleNavierStokesBase::set_time_evolving_vars(system);
  }


  void AveragedTurbine::read_input_options( const GetPot& input )
  {
    std::string base_function =
      input("Physics/"+averaged_turbine+"/base_velocity",
        std::string("0"));

    if (base_function == "0")
      libmesh_error_msg("Error! Zero AveragedTurbine specified!" <<
                        std::endl);

    if (base_function == "0")
      this->base_velocity_function.reset
        (new libMesh::ZeroFunction<libMesh::Number>());
    else
      this->base_velocity_function.reset
        (new libMesh::ParsedFunction<libMesh::Number>(base_function));

    std::string vertical_function =
      input("Physics/"+averaged_turbine+"/local_vertical",
        std::string("0"));

    if (vertical_function == "0")
      libmesh_error_msg("Warning! Zero LocalVertical specified!" <<
                        std::endl);

    this->local_vertical_function.reset
      (new libMesh::ParsedFunction<libMesh::Number>(vertical_function));

    std::string lift_function_string =
      input("Physics/"+averaged_turbine+"/lift",
        std::string("0"));

    if (lift_function_string == "0")
      std::cout << "Warning! Zero lift function specified!" << std::endl;

    this->lift_function.reset
      (new libMesh::ParsedFunction<libMesh::Number>(lift_function_string));

    std::string drag_function_string =
      input("Physics/"+averaged_turbine+"/drag",
        std::string("0"));

    if (drag_function_string == "0")
      std::cout << "Warning! Zero drag function specified!" << std::endl;

    this->drag_function.reset
      (new libMesh::ParsedFunction<libMesh::Number>(drag_function_string));

    std::string chord_function_string =
      input("Physics/"+averaged_turbine+"/chord_length",
        std::string("0"));

    if (chord_function_string == "0")
      libmesh_error_msg("Warning! Zero chord function specified!" <<
                        std::endl);

    this->chord_function.reset
      (new libMesh::ParsedFunction<libMesh::Number>(chord_function_string));

    std::string area_function_string =
      input("Physics/"+averaged_turbine+"/area_swept",
        std::string("0"));

    if (area_function_string == "0")
      libmesh_error_msg("Warning! Zero area_swept_function specified!" <<
                        std::endl);

    this->area_swept_function.reset
      (new libMesh::ParsedFunction<libMesh::Number>(area_function_string));

    std::string aoa_function_string =
      input("Physics/"+averaged_turbine+"/angle_of_attack",
        std::string("00000"));

    if (aoa_function_string == "00000")
      libmesh_error_msg("Warning! No angle-of-attack specified!" <<
                        std::endl);

    this->aoa_function.reset
      (new libMesh::ParsedFunction<libMesh::Number>(aoa_function_string));

    std::string torque_function_string =
      input("Physics/"+averaged_turbine+"/torque",
        std::string("0"));

    if (torque_function_string == "0")
      std::cout << "Warning! Zero torque function specified!" << std::endl;

    this->torque_function.reset
      (new libMesh::ParsedFunction<libMesh::Number>(torque_function_string));

    moment_of_inertia = input("Physics/"+averaged_turbine+"/moment_of_inertia",
                              libMesh::Number(0));

    if (!moment_of_inertia)
      libmesh_error_msg(
        "Error! Zero AveragedTurbine moment of inertia specified!" <<
        std::endl);

    initial_speed = input("Physics/"+averaged_turbine+"/initial_speed",
                          libMesh::Number(0));

    _fan_speed_var_name = input("Physics/VariableNames/fan_speed",
                            fan_speed_var_name_default);
  }

  void AveragedTurbine::element_time_derivative( bool compute_jacobian,
					        AssemblyContext& context,
					        CachedValues& /* cache */ )
  {
#ifdef GRINS_USE_GRVY_TIMERS
    this->_timer->BeginTimer("AveragedTurbine::element_time_derivative");
#endif

    // Element Jacobian * quadrature weights for interior integration
    const std::vector<libMesh::Real> &JxW = 
      context.get_element_fe(this->_flow_vars.u_var())->get_JxW();

    // The shape functions at interior quadrature points.
    const std::vector<std::vector<libMesh::Real> >& u_phi = 
      context.get_element_fe(this->_flow_vars.u_var())->get_phi();

    const std::vector<libMesh::Point>& u_qpoint = 
      context.get_element_fe(this->_flow_vars.u_var())->get_xyz();

    // The number of local degrees of freedom in each variable
    const unsigned int n_u_dofs = context.get_dof_indices(_flow_vars.u_var()).size();

    // The subvectors and submatrices we need to fill:
    libMesh::DenseSubMatrix<libMesh::Number> &Kuu = context.get_elem_jacobian(_flow_vars.u_var(), _flow_vars.u_var()); // R_{u},{u}
    libMesh::DenseSubMatrix<libMesh::Number> &Kuv = context.get_elem_jacobian(_flow_vars.u_var(), _flow_vars.v_var()); // R_{u},{v}
    libMesh::DenseSubMatrix<libMesh::Number> &Kvu = context.get_elem_jacobian(_flow_vars.v_var(), _flow_vars.u_var()); // R_{v},{u}
    libMesh::DenseSubMatrix<libMesh::Number> &Kvv = context.get_elem_jacobian(_flow_vars.v_var(), _flow_vars.v_var()); // R_{v},{v}

    libMesh::DenseSubMatrix<libMesh::Number> &Kus =
            context.get_elem_jacobian(_flow_vars.u_var(), _fan_speed_var); // R_{u},{s}
    libMesh::DenseSubMatrix<libMesh::Number> &Ksu =
            context.get_elem_jacobian(_fan_speed_var, _flow_vars.u_var()); // R_{s},{u}
    libMesh::DenseSubMatrix<libMesh::Number> &Kvs =
            context.get_elem_jacobian(_flow_vars.v_var(), _fan_speed_var); // R_{v},{s}
    libMesh::DenseSubMatrix<libMesh::Number> &Ksv =
            context.get_elem_jacobian(_fan_speed_var, _flow_vars.v_var()); // R_{s},{v}
    libMesh::DenseSubMatrix<libMesh::Number> &Kss =
            context.get_elem_jacobian(_fan_speed_var, _fan_speed_var); // R_{s},{s}

    libMesh::DenseSubMatrix<libMesh::Number>* Kwu = NULL;
    libMesh::DenseSubMatrix<libMesh::Number>* Kwv = NULL;
    libMesh::DenseSubMatrix<libMesh::Number>* Kww = NULL;
    libMesh::DenseSubMatrix<libMesh::Number>* Kuw = NULL;
    libMesh::DenseSubMatrix<libMesh::Number>* Kvw = NULL;

    libMesh::DenseSubMatrix<libMesh::Number>* Ksw = NULL;
    libMesh::DenseSubMatrix<libMesh::Number>* Kws = NULL;

    libMesh::DenseSubVector<libMesh::Number> &Fu = context.get_elem_residual(_flow_vars.u_var()); // R_{u}
    libMesh::DenseSubVector<libMesh::Number> &Fv = context.get_elem_residual(_flow_vars.v_var()); // R_{v}
    libMesh::DenseSubVector<libMesh::Number>* Fw = NULL;

    libMesh::DenseSubVector<libMesh::Number> &Fs = context.get_elem_residual(_fan_speed_var); // R_{s}

    if( this->_dim == 3 )
      {
        Kuw = &context.get_elem_jacobian(_flow_vars.u_var(), _flow_vars.w_var()); // R_{u},{w}
        Kvw = &context.get_elem_jacobian(_flow_vars.v_var(), _flow_vars.w_var()); // R_{v},{w}

        Kwu = &context.get_elem_jacobian(_flow_vars.w_var(), _flow_vars.u_var()); // R_{w},{u}
        Kwv = &context.get_elem_jacobian(_flow_vars.w_var(), _flow_vars.v_var()); // R_{w},{v}
        Kww = &context.get_elem_jacobian(_flow_vars.w_var(), _flow_vars.w_var()); // R_{w},{w}

        Ksw = &context.get_elem_jacobian(_fan_speed_var, _flow_vars.w_var()); // R_{s},{w}
        Kws = &context.get_elem_jacobian(_flow_vars.w_var(), _fan_speed_var); // R_{w},{s}

        Fw  = &context.get_elem_residual(_flow_vars.w_var()); // R_{w}
      }

    unsigned int n_qpoints = context.get_element_qrule().n_points();

    for (unsigned int qp=0; qp != n_qpoints; qp++)
      {
        // Compute the solution & its gradient at the old Newton iterate.
        libMesh::Number u, v, s;
        u = context.interior_value(_flow_vars.u_var(), qp);
        v = context.interior_value(_flow_vars.v_var(), qp);
        s = context.interior_value(_fan_speed_var, qp);

        libMesh::NumberVectorValue U(u,v);
        if (_dim == 3)
          U(2) = context.interior_value(_flow_vars.w_var(), qp); // w

        // Find base velocity of moving fan at this point
        libmesh_assert(base_velocity_function.get());

        libMesh::DenseVector<libMesh::Number> output_vec(3);

        (*base_velocity_function)(u_qpoint[qp], context.time,
                                  output_vec);

        const libMesh::NumberVectorValue U_B_1(output_vec(0),
                                               output_vec(1),
                                               output_vec(2));

        const libMesh::NumberVectorValue U_B = U_B_1 * s;

        const libMesh::Number U_B_size = U_B.size();

        // Normal in fan velocity direction
        const libMesh::NumberVectorValue N_B = U_B_size ?
                libMesh::NumberVectorValue(U_B/U_B.size()) : U_B;

        (*local_vertical_function)(u_qpoint[qp], context.time,
                                   output_vec);

        // Normal in fan vertical direction
        const libMesh::NumberVectorValue N_V(output_vec(0),
                                             output_vec(1),
                                             output_vec(2));

        // Normal in radial direction (or opposite radial direction,
        // for fans turning clockwise!)
        const libMesh::NumberVectorValue N_R = N_B.cross(N_V);

        // Fan-wing-plane component of local relative velocity
        const libMesh::NumberVectorValue U_P = U - (U*N_R)*N_R - U_B;

        const libMesh::Number U_P_size = U_P.size();

        // Direction opposing drag
        const libMesh::NumberVectorValue N_drag = U_P_size ?
                libMesh::NumberVectorValue(-U_P/U_P_size) : U_P;

        // Direction opposing lift
        const libMesh::NumberVectorValue N_lift = N_drag.cross(N_R);

        // "Forward" velocity
        const libMesh::Number u_fwd = -(U_P * N_B);

        // "Upward" velocity
        const libMesh::Number u_up = U_P * N_V;

        // Angle WRT fan velocity direction
        const libMesh::Number part_angle = (u_up || u_fwd) ?
                std::atan2(u_up, u_fwd) : 0;

        // Angle WRT fan chord
        const libMesh::Number angle = part_angle +
          (*aoa_function)(u_qpoint[qp], context.time);

        const libMesh::Number C_lift  = (*lift_function)(u_qpoint[qp], angle);
        const libMesh::Number C_drag  = (*drag_function)(u_qpoint[qp], angle);

        const libMesh::Number d_C_lift_d_angle = compute_jacobian ? 
          ((*lift_function)(u_qpoint[qp], angle+libMesh::TOLERANCE) -
           (*lift_function)(u_qpoint[qp], angle-libMesh::TOLERANCE)) / 
          (2 * libMesh::TOLERANCE) :
           0;
        const libMesh::Number d_C_drag_d_angle = compute_jacobian ? 
          ((*drag_function)(u_qpoint[qp], angle+libMesh::TOLERANCE) -
           (*drag_function)(u_qpoint[qp], angle-libMesh::TOLERANCE)) / 
          (2 * libMesh::TOLERANCE) :
           0;

        const libMesh::Number chord = (*chord_function)(u_qpoint[qp], context.time);
        const libMesh::Number area  = (*area_swept_function)(u_qpoint[qp], context.time);

        const libMesh::Number v_sq = U_P*U_P;

        const libMesh::Number dV2_ds = compute_jacobian ?
          -2 * (U_P * U_B) : 0;
        const libMesh::Number UPNR = compute_jacobian ?
          U_P*N_R : 0;

        const libMesh::Number half_rho_chord_over_area =
          0.5 * this->_rho * chord / area;
        const libMesh::Number LDfactor = half_rho_chord_over_area * v_sq;
        const libMesh::Number lift = C_lift * LDfactor;
        const libMesh::Number drag = C_drag * LDfactor;

        // Force 
        const libMesh::NumberVectorValue F = lift * N_lift + drag * N_drag;

        // Using this dot product to derive torque *depends* on s=1
        // and U_B_1 corresponding to 1 rad/sec base velocity; this
        // means that the length of U_B_1 is equal to radius.

        // F is the force on the air, so *negative* F is the force on
        // the turbine.
        Fs(0) -= U_B_1 * F * JxW[qp];

        if (compute_jacobian)
          {
            const libMesh::Number LDderivfactor = 
                -(N_lift*C_lift+N_drag*C_drag) *
                U_B_1 * half_rho_chord_over_area *
                JxW[qp];

            Kss(0,0) += LDderivfactor * dV2_ds;

            const libMesh::Number angle_derivfactor =
              (N_lift * d_C_lift_d_angle +
               N_drag * d_C_drag_d_angle) *
              U_B_1 * LDfactor *
              JxW[qp];

            Kss(0,0) += angle_derivfactor *
              (u_up * (U_B_1*N_B) - u_fwd * (U_B_1*N_V))/v_sq;

            const libMesh::NumberVectorValue
              d_angle_dU = (u_fwd * N_V -
                            u_up * N_B)/v_sq;

            for (unsigned int j=0; j != n_u_dofs; j++)
              {
                const libMesh::Number
                  dV2_du = 2 * u_phi[j][qp] *
                           (U_P(0) - N_R(0)*UPNR);
                const libMesh::Number
                  dV2_dv = 2 * u_phi[j][qp] *
                           (U_P(1) - N_R(1)*UPNR);

                Ksu(0,j) += LDderivfactor * dV2_du;
                Ksv(0,j) += LDderivfactor * dV2_dv;

                const libMesh::Number
                  d_angle_du = d_angle_dU(0) * u_phi[j][qp];

                const libMesh::Number
                  d_angle_dv = d_angle_dU(1) * u_phi[j][qp];

                Ksu(0,j) += angle_derivfactor * d_angle_du;
                Ksv(0,j) += angle_derivfactor * d_angle_dv;

                if (_dim == 3)
                  {
                    const libMesh::Number
                      dV2_dw = 2 * u_phi[j][qp] *
                               (U_P(2) - N_R(2)*UPNR);

                    (*Ksw)(0,j) += LDderivfactor * dV2_dw;

                    const libMesh::Number
                      d_angle_dw = d_angle_dU(2) * u_phi[j][qp];

                    (*Ksw)(0,j) += angle_derivfactor * d_angle_dw;
                  }

              } // End j dof loop
          }

        for (unsigned int i=0; i != n_u_dofs; i++)
          {
            Fu(i) += F(0)*u_phi[i][qp]*JxW[qp];
            Fv(i) += F(1)*u_phi[i][qp]*JxW[qp];

            if (_dim == 3)
              (*Fw)(i) += F(2)*u_phi[i][qp]*JxW[qp];

            if (compute_jacobian)
              {
                const libMesh::NumberVectorValue
                  LDderivfactor = 
                    (N_lift*C_lift+N_drag*C_drag) *
                    (half_rho_chord_over_area *
                     u_phi[i][qp]*JxW[qp]);

                Kus(i,0) += LDderivfactor(0) * dV2_ds;
                Kvs(i,0) += LDderivfactor(1) * dV2_ds;
                if (_dim == 3)
                  (*Kws)(i,0) += LDderivfactor(2) * dV2_ds;

                const libMesh::NumberVectorValue
                  angle_derivfactor =
                    (N_lift * d_C_lift_d_angle +
                     N_drag * d_C_drag_d_angle) *
                    (LDfactor * u_phi[i][qp]*JxW[qp]);

                const libMesh::Number d_angle_ds =
                  (u_up * (U_B_1*N_B) - u_fwd * (U_B_1*N_V))/v_sq;

                Kus(i,0) += angle_derivfactor(0) * d_angle_ds;
                Kvs(i,0) += angle_derivfactor(1) * d_angle_ds;
                if (_dim == 3)
                  (*Kws)(i,0) += angle_derivfactor(2) * d_angle_ds;

                const libMesh::NumberVectorValue
                  d_angle_dU = (u_fwd * N_V -
                                u_up * N_B)/v_sq;

                for (unsigned int j=0; j != n_u_dofs; j++)
                  {
                    const libMesh::Number
                      dV2_du = 2 * u_phi[j][qp] *
                               (U_P(0) - N_R(0)*UPNR);
                    const libMesh::Number
                      dV2_dv = 2 * u_phi[j][qp] *
                               (U_P(1) - N_R(1)*UPNR);

                    Kuu(i,j) += LDderivfactor(0) * dV2_du;
                    Kuv(i,j) += LDderivfactor(0) * dV2_dv;
                    Kvu(i,j) += LDderivfactor(1) * dV2_du;
                    Kvv(i,j) += LDderivfactor(1) * dV2_dv;

                    const libMesh::Number
                      d_angle_du = d_angle_dU(0) * u_phi[j][qp];

                    const libMesh::Number
                      d_angle_dv = d_angle_dU(1) * u_phi[j][qp];

                    Kuu(i,j) += angle_derivfactor(0) * d_angle_du;
                    Kuv(i,j) += angle_derivfactor(0) * d_angle_dv;
                    Kvu(i,j) += angle_derivfactor(1) * d_angle_du;
                    Kvv(i,j) += angle_derivfactor(1) * d_angle_dv;

                    if (_dim == 3)
                      {
                        const libMesh::Number
                          dV2_dw = 2 * u_phi[j][qp] *
                                   (U_P(2) - N_R(2)*UPNR);

                        const libMesh::Number
                          d_angle_dw = d_angle_dU(2) * u_phi[j][qp];

                        (*Kuw)(i,j) += LDderivfactor(0) * dV2_dw;
                        (*Kvw)(i,j) += LDderivfactor(1) * dV2_dw;
                        (*Kwu)(i,j) += LDderivfactor(2) * dV2_du;
                        (*Kwv)(i,j) += LDderivfactor(2) * dV2_dv;
                        (*Kww)(i,j) += LDderivfactor(2) * dV2_dw;

                        (*Kuw)(i,j) += angle_derivfactor(0) * d_angle_dw;
                        (*Kvw)(i,j) += angle_derivfactor(1) * d_angle_dw;
                        (*Kwu)(i,j) += angle_derivfactor(2) * d_angle_du;
                        (*Kwv)(i,j) += angle_derivfactor(2) * d_angle_dv;
                        (*Kww)(i,j) += angle_derivfactor(2) * d_angle_dw;
                      }

                  } // End j dof loop
              } // End compute_jacobian check

          } // End i dof loop

      }

#ifdef GRINS_USE_GRVY_TIMERS
    this->_timer->EndTimer("AveragedTurbine::element_time_derivative");
#endif

    return;
  }



  void AveragedTurbine::nonlocal_time_derivative(bool compute_jacobian,
				                 AssemblyContext& context,
				                 CachedValues& /* cache */ )
  {
    libMesh::DenseSubMatrix<libMesh::Number> &Kss =
            context.get_elem_jacobian(_fan_speed_var, _fan_speed_var); // R_{s},{s}

    libMesh::DenseSubVector<libMesh::Number> &Fs =
            context.get_elem_residual(_fan_speed_var); // R_{s}

    const std::vector<libMesh::dof_id_type>& dof_indices =
      context.get_dof_indices(_fan_speed_var);

    const libMesh::Number fan_speed =
      context.get_system().current_solution(dof_indices[0]);

    const libMesh::Number output_torque =
      (*torque_function)(libMesh::Point(0), fan_speed);

    Fs(0) += output_torque;

    if (compute_jacobian && context.elem_solution_derivative)
      {
        // FIXME: we should replace this FEM with a hook to the AD fparser stuff
        const libMesh::Number epsilon = 1e-6;
        const libMesh::Number output_torque_deriv =
          ((*torque_function)(libMesh::Point(0), fan_speed+epsilon) -
           (*torque_function)(libMesh::Point(0), fan_speed-epsilon)) / (2*epsilon);

        libmesh_assert_equal_to (context.elem_solution_derivative, 1.0);

        Kss(0,0) += output_torque_deriv;
      }

    return;
  }



  void AveragedTurbine::nonlocal_mass_residual( bool compute_jacobian,
				                AssemblyContext& context,
				                CachedValues& /* cache */ )
  {
    libMesh::DenseSubMatrix<libMesh::Number> &Kss =
            context.get_elem_jacobian(_fan_speed_var, _fan_speed_var); // R_{s},{s}

    libMesh::DenseSubVector<libMesh::Number> &Fs =
            context.get_elem_residual(_fan_speed_var); // R_{s}

    const libMesh::DenseSubVector<libMesh::Number> &Us =
      context.get_elem_solution(_fan_speed_var);

    const libMesh::Number& fan_speed = Us(0);

    Fs(0) += moment_of_inertia * fan_speed;

    if (compute_jacobian && context.elem_solution_derivative)
      {
        libmesh_assert_equal_to (context.elem_solution_derivative, 1.0);

        Kss(0,0) += moment_of_inertia;
      }

    return;
  }

} // namespace GRINS