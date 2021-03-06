/*
 * © All rights reserved. ECOLE POLYTECHNIQUE FEDERALE DE LAUSANNE, Switzerland, 
 * Laboratory ANMC, 2016
 * See the LICENSE.TXT file for more details.
 * 
 * Authors
 * The original Fortran codes were developed by Assyr Abdulle (for ROCK2) and 
 * Sommeijer, Shampine, Verwer (for RKC). Translation to C++ by Giacomo Rosilho
 * de Souza.
 * 
 * The ROCK2 code is described in 
 * 
 * Abdulle, Assyr, and Alexei A. Medovikov. 
 * "Second order Chebyshev methods based on orthogonal polynomials." 
 * Numerische Mathematik 90.1 (2001): 1-18.	
 * 
 * and the RKC code in
 * 
 * Sommeijer, B. P., L. F. Shampine, and J. G. Verwer. 
 * "RKC: An explicit solver for parabolic PDEs." 
 * Journal of Computational and Applied Mathematics 88.2 (1998): 315-326.
 * 
 * Please cite these articles in any publication describing research
 * performed using the software.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <stdio.h>

#include "TimeIntegrator.h"
#include "ChebyshevIntegrators.h"
#include "Ode.h"
#include "Mesh.h"
#include "problem.h"
#include "ImplicitIntegrator.h"

int main(int argc, char** argv)
{
//----------------    DEFAULT ACCURACY PARAMETERS   ----------------------------
    int n_ref = 2;    		//mesh size or mesh name depending if non local or local
    double dt =  0.0005;		//initial step size
    double rtol= 1.0e-2;	//relative tolerance
    double atol= rtol; 	      	//absolute tolerance

//----------------    DEFAULT INTEGRATION OPTIONS   ----------------------------
    bool dt_adaptivity = false; 	//time step adaptivity enabled/disabled
    bool one_step=true;         	//advance one time step per ROCK2 call
    bool intrho=true;			//spectral radius computed internally in the time integrator
    double tend=10;			//last time

//-----    READ OPTIONAL INPUT ACCURACY/INTEGRATION PARAMETERS   ---------------
    if(argc>1){
        std::istringstream Nbuf(argv[1]);
        std::istringstream dtbuf(argv[2]);
        Nbuf>>n_ref;
        dtbuf>>dt;
        dt_adaptivity= (*argv[3]=='1');
        one_step= (*argv[4]=='1');
    }

//-------------------    MESH INITIALIZATION  ----------------------------------
    double dx = 0.01;			//grid space
    Mesh mesh(dx);			//initialize the mesh
    mesh.print_info();

//-----------------    INITIAL DATA DEFINITION  --------------------------------
    std::vector<double> pot_initial(mesh.n_elem,-64.974);
    double alpha_n(0.01*(-(-64.974+55.0))/(exp(-(-64.974+55)/10)-1));
    double beta_n(0.125*exp(-(-64.974+65)/80));
    double alpha_m(0.1*(-(-64.974+40))/(exp(-(-64.974+40)/10)-1));
    double beta_m(4*exp(-(-64.974+65)/18));
    double alpha_h(0.07*exp(-(-64.974+65)/20));
    double beta_h(1/(exp(-(-64.974+35)/10)+1));
    std::vector<double> n_initial(mesh.n_elem,alpha_n/(alpha_n+beta_n));
    std::vector<double> m_initial(mesh.n_elem,alpha_m/(alpha_m+beta_m));
    std::vector<double> h_initial(mesh.n_elem,alpha_h/(alpha_h+beta_h));

//-------------------    PROBLEM INITIALIZATION  -------------------------------
    CABLE* cable = new CABLE(mesh,intrho,tend,pot_initial);
    GATE_N* gate_n = new GATE_N(mesh,intrho,tend,n_initial);
    GATE_M* gate_m = new GATE_M(mesh,intrho,tend,m_initial);
    GATE_H* gate_h = new GATE_H(mesh,intrho,tend,h_initial);
    gate_n->get_potential(cable->un);
    gate_m->get_potential(cable->un);
    gate_h->get_potential(cable->un);
    cable->get_gate_state(gate_n->un,gate_m->un,gate_h->un);

//---------------------    ROCK2/RKC INITIALIZATION  ---------------------------
    bool verbose=true; 
    ROCK2 rock_cable(one_step, verbose, dt_adaptivity, atol, rtol, intrho);
    //CN cn_cable(cable,one_step, verbose);
    rock_cable.print_info();
    verbose=true;
    ROCK2 rock_gate_n(one_step, verbose, dt_adaptivity, atol, rtol, intrho);  
    ROCK2 rock_gate_m(one_step, verbose, dt_adaptivity, atol, rtol, intrho);
    ROCK2 rock_gate_h(one_step, verbose, dt_adaptivity, atol, rtol, intrho);
//-------------------------   TIME LOOP   --------------------------------------

    if(rock_cable.check_correctness(dt)==0 && rock_gate_n.check_correctness(dt)==0
       && rock_gate_m.check_correctness(dt)==0 && rock_gate_h.check_correctness(dt)==0) //checks if given parameters are ok
        return 0;//something is wrong
    int idid = 2;
    FILE * monitor_ending_branchA_potential;
    monitor_ending_branchA_potential = fopen ("../output/monitor_ending_branchA_potential.txt","w");
    fprintf(monitor_ending_branchA_potential,"%.8f \t %.8f \n",cable->time,cable->un[mesh.n_L1+mesh.n_L2-5]);
    dt=dt/2;
    rock_gate_n.advance(gate_n,dt,idid);
    rock_gate_m.advance(gate_m,dt,idid);
    rock_gate_h.advance(gate_h,dt,idid);
    dt=dt*2;    
    idid=2;
fflush(stdout);

    for(int iter=0;(idid==2)&&(cable->time<=tend);++iter)
    {
        rock_cable.advance(cable,dt,idid);
        //cn_cable.advance(cable,dt,idid);
        fprintf(monitor_ending_branchA_potential,"%.12f \t %.12f \n",cable->time,cable->un[mesh.n_L1+mesh.n_L2-5]);
        rock_gate_n.advance(gate_n,dt,idid);
        rock_gate_m.advance(gate_m,dt,idid);
        rock_gate_h.advance(gate_h,dt,idid);
    }   
   fclose (monitor_ending_branchA_potential);
   rock_cable.print_statistics();
//---------------------------   WRITE ON SCREEN   ----------------------------------------
   bool print_GNU=true;
   if (print_GNU)
   {
   FILE * branchA_potential;
   FILE * branchB_potential;
   FILE * branchC_potential;
   FILE * branchA_gate_n;
   FILE * branchB_gate_n;
   FILE * branchC_gate_n;
   FILE * branchA_gate_m;
   FILE * branchB_gate_m;
   FILE * branchC_gate_m;
   FILE * branchA_gate_h;
   FILE * branchB_gate_h;
   FILE * branchC_gate_h;
   branchA_potential = fopen ("../output/branchA_potential.txt","w");
   branchB_potential = fopen ("../output/branchB_potential.txt","w");
   branchC_potential = fopen ("../output/branchC_potential.txt","w");
   branchA_gate_n = fopen ("../output/branchA_gate_n.txt","w");
   branchB_gate_n = fopen ("../output/branchB_gate_n.txt","w");
   branchC_gate_n = fopen ("../output/branchC_gate_n.txt","w");
   branchA_gate_m = fopen ("../output/branchA_gate_m.txt","w");
   branchB_gate_m = fopen ("../output/branchB_gate_m.txt","w");
   branchC_gate_m = fopen ("../output/branchC_gate_m.txt","w");
   branchA_gate_h = fopen ("../output/branchA_gate_h.txt","w");
   branchB_gate_h = fopen ("../output/branchB_gate_h.txt","w");
   branchC_gate_h = fopen ("../output/branchC_gate_h.txt","w");
    for (std::size_t i=0; i<(mesh.n_L1-1); i++){
      fprintf(branchA_potential,"%.12f \t %.12f \n",mesh.grid[i],cable->un[i]);
      fprintf(branchA_gate_n,"%.12f \t %.12f \n",mesh.grid[i],gate_n->un[i]);
      fprintf(branchA_gate_m,"%.12f \t %.12f \n",mesh.grid[i],gate_m->un[i]);
      fprintf(branchA_gate_h,"%.12f \t %.12f \n",mesh.grid[i],gate_h->un[i]);
    }
   fclose (branchA_potential);
   fclose (branchA_gate_n);
   fclose (branchA_gate_m);
   fclose (branchA_gate_h);
    for (std::size_t i=(mesh.n_L1-1); i<(mesh.n_L1+mesh.n_L2-2); i++){
      fprintf(branchB_potential,"%.12f \t %.12f \n",mesh.grid[i],cable->un[i]);
      fprintf(branchB_gate_n,"%.12f \t %.12f \n",mesh.grid[i],gate_n->un[i]);
      fprintf(branchB_gate_m,"%.12f \t %.12f \n",mesh.grid[i],gate_m->un[i]);
      fprintf(branchB_gate_h,"%.12f \t %.12f \n",mesh.grid[i],gate_h->un[i]);
    }
   fclose (branchB_potential);
   fclose (branchB_gate_n);
   fclose (branchB_gate_m);
   fclose (branchB_gate_h);
    for (std::size_t i=(mesh.n_L1+mesh.n_L2-2); i<mesh.n_elem; i++){
      fprintf(branchC_potential,"%.12f \t %.12f \n",mesh.grid[i],cable->un[i]);
      fprintf(branchC_gate_n,"%.12f \t %.12f \n",mesh.grid[i],gate_n->un[i]);
      fprintf(branchC_gate_m,"%.12f \t %.12f \n",mesh.grid[i],gate_m->un[i]);
      fprintf(branchC_gate_h,"%.12f \t %.12f \n",mesh.grid[i],gate_h->un[i]);
    }
   fclose (branchC_potential);
   fclose (branchC_gate_n);
   fclose (branchC_gate_m);
   fclose (branchC_gate_h);
    }
//------------------------   DEALLOCATE   -------------------------------------    
//    delete heat;
    return 0;
}

