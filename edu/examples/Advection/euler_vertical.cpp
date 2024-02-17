
// g++ -o euler1dv.exe -I/usr/local/include euler_vertical.cpp

/// The armadillo library is to allow the use of 3d cubes and other
/// array types, with array math built in. This eliminates loops!
#include <armadillo>

/// This is used for timing and the random seed generator:
#include <chrono>

// Types
// Precision compile-time aliasing
//#ifdef AETHER_USE_PRECISION_DOUBLE
/// Precision type chosen to be `double` through `AETHER_USE_PRECISION_DOUBLE`
using precision_t = double;
//#else
/// Precision type compile-time default to float.
//using precision_t = float;
//#endif

/// Armadillo type vector (single column) with compile-time precision.
using arma_vec = arma::Col<precision_t>;
/// Armadillo type matrix (two dimension) with compile-time precision.
using arma_mat = arma::Mat<precision_t>;
/// Armadillo type cube (three dimension) with compile-time precision.
using arma_cube = arma::Cube<precision_t>;

#include <fstream>

const precision_t t0 = 1000.0;
const precision_t mass = 16.0 * 1.67e-27;
const precision_t r0 = 1.0e19 * mass;
const precision_t kb = 1.38e-23;
const precision_t gravity = -kb * t0 / mass / 100000.0;

// ---------------------------------------------------------
// grid stretched creation
// ---------------------------------------------------------

arma_vec init_stretched_grid(int64_t nPts, int64_t nGCs) {

  precision_t dx = 1.0;
  arma_vec x(nPts + nGCs * 2);

  precision_t factor = 1.0;
  precision_t i2pi = 2.0 * 3.1415927 / (nPts-1);

  x(nGCs) = 0.0;
  
  for (int64_t i = 1; i < nPts + nGCs; i++) {
    x(i + nGCs) = x(i - 1 + nGCs) + dx  + factor * (1 + cos(i * i2pi));
    std::cout << "i : " << i << " " << cos(i * i2pi) << "\n";
  }
  for (int64_t i = -1; i >= -nGCs; i--) {
    x(i + nGCs) = x(i + 1 + nGCs) - dx  - factor * (1 + cos(i * i2pi));
    std::cout << "i : " << i << " " << cos(i * i2pi) << "\n";
  }
  precision_t maxX = x(nPts + nGCs - 1);
  x = 100.0 * x / maxX;
  
  return x;
}

// ---------------------------------------------------------
// grid creation
// ---------------------------------------------------------

arma_vec init_grid(int64_t nPts, int64_t nGCs) {

  precision_t dx = 1.0 / nPts;
  arma_vec x(nPts + nGCs * 2);

  // uniform grid:
  for (int64_t i = -nGCs; i < nPts + nGCs; i++) {
    x(i + nGCs) = i * dx;
  }
  // stretch to be 100 km:
  x = x * 200.0 * 1000.0;

  return x;
}

// ---------------------------------------------------------
// bin edges
// ---------------------------------------------------------

arma_vec calc_bin_edges(arma_vec centers) {

  int64_t nPts = centers.n_elem;
  arma_vec edges(nPts+1);

  precision_t dc = centers(1) - centers(0);

  edges(0) = centers(0) - dc / 2.0;
  edges(1) = centers(0) + dc / 2.0;
  for (int64_t i = 2; i < nPts + 1; i++)
    edges(i) = 2 * centers(i - 1) - edges(i - 1);
  
  return edges;
}

// ---------------------------------------------------------
// bin widths
// ---------------------------------------------------------

arma_vec calc_bin_widths(arma_vec edges) {

  int64_t nPts = edges.n_elem - 1;
  arma_vec widths(nPts);

  for (int64_t i = 0; i < nPts; i++)
    widths(i) = edges(i + 1) - edges(i);
  
  return widths;
}

// ---------------------------------------------------------
// initial rho
// ---------------------------------------------------------

arma_vec init_rho(int64_t nPts, arma_vec x) {

  arma_vec rho(nPts);
  precision_t h, dx;
  rho(0) = r0;
  for (int64_t i = 1; i < nPts; i++) {
    // t = 100:
    h = kb * t0 / mass / abs(gravity);
    dx = x(i) - x(i-1);
    rho(i) = rho(i-1) * exp( - dx / h);
//std::cout << "i, rho : " << i
//	      << " " << rho(i)
//	      << " " << dx
//	      << " " << h << "\n";
  }

  return rho;
}

// ---------------------------------------------------------
// set BCs
// ---------------------------------------------------------

void set_bcs(int64_t nPts, int64_t nGCs,
	     arma_vec x,
	     arma_vec &rho,
	     arma_vec &vel,
	     arma_vec &temp) {

  precision_t h, dx;
  // Lower BC on rho:
  rho(0) = r0;
  vel(0) = 0.0; //vel(nGCs);
  temp(0) = t0;
  for (int64_t i = 1; i < nGCs; i++) {
    h = kb * t0 / mass / abs(gravity);
    dx = x(i) - x(i-1);
    rho(i) = rho(i-1) * exp( - dx / h);
    vel(i) = 0.0; //vel(nGCs);
    temp(i) = t0;
  }
  // Upper BC on rho:
  for (int64_t i = nPts + nGCs; i < nPts + 2 * nGCs; i++) {
    h = kb * temp(i) / mass / abs(gravity);
    dx = x(i) - x(i-1);
    temp(i) = temp(i-1);
    rho(i) = temp(i-1) / temp(i) * rho(i-1) * exp( - dx / h);
    //if (vel(i-1) >= 0.0) {
    vel(i) = vel(i-1);
      //} else {
      //vel(i) = 0.0;
      //}
  }

  return;
}

// ---------------------------------------------------------
// initial velocity
// ---------------------------------------------------------

arma_vec init_vel(int64_t nPts) {
  arma_vec vel(nPts);
  // all cells positive to right:
  vel.zeros();
  return vel;
}

// ---------------------------------------------------------
// initial temp (e)
// ---------------------------------------------------------

arma_vec init_temp(int64_t nPts) {
  arma_vec temp(nPts);
  temp.ones();
  temp = temp * t0;
  return temp;
}

// ---------------------------------------------------------
// exchange messages
// ---------------------------------------------------------

void exchange(arma_vec &values, int64_t nPts, int64_t nGCs) {

  int64_t iEnd = nPts + 2 * nGCs;
  // this is a periodic BC:
  for (int64_t i = 0; i < nGCs; i++) {
    values(i) = values(iEnd - 2 * nGCs + i);
    values(iEnd - nGCs + i) = values(nGCs + i);
  }
}

// ---------------------------------------------------------
//
// ---------------------------------------------------------

arma_vec limiter_mc(arma_vec left,
		    arma_vec right,
		    int64_t nPts,
		    int64_t nGCs) {

  precision_t beta = 0.8;

  arma_vec s = left % right;
  arma_vec combined = (left + right) * 0.5;

  left = left * beta;
  right = right * beta;
  arma_vec limited = left;
  
  for (int64_t i = 1; i < nPts + 2 * nGCs - 1; i++) {
    if (s(i) < 0) {
      // Sign < 0 means opposite signed left and right:
      limited(i) = 0.0;
    } else {
      if (left(i) > 0 && right(i) > 0) {
	if (right(i) < limited(i))
	  limited(i) = right(i);
	if (combined(i) < limited(i))
	  limited(i) = combined(i);
      } else {
	if (right(i) > limited(i))
	  limited(i) = right(i);
	if (combined(i) > limited(i))
	  limited(i) = combined(i);
      }
    }

  }
  return limited;
}

// ---------------------------------------------------------
// calc gradients at centers
//   - values and x defined at centers
// ---------------------------------------------------------

arma_vec calc_grad(arma_vec values,
		   arma_vec x,
		   int64_t nPts,
		   int64_t nGCs) {

  arma_vec gradients = values * 0.0;
  arma_vec gradL = values * 0.0;
  arma_vec gradR = values * 0.0;

  precision_t factor1 = 0.625;
  precision_t factor2 = 0.0416667;
  precision_t h;

  int64_t i;
  
  i = nGCs - 1;
  h = 2.0 / (x(i+1) - x(i));
  gradR(i) = h * (factor1 * (values(i+1) - values(i)) -
		  factor2 * (values(i+2) - values(i-1)));
  gradL(i) = (values(i) - values(i-1)) / (x(i) - x(i-1));
  
  for (i = nGCs; i < nPts + nGCs; i++) {
    h = 2.0 / (x(i) - x(i-1));
    gradL(i) = h * (factor1 * (values(i) - values(i-1)) -
		    factor2 * (values(i+1) - values(i-2)));
    h = 2.0 / (x(i+1) - x(i));
    gradR(i) = h * (factor1 * (values(i+1) - values(i)) -
		    factor2 * (values(i+2) - values(i-1)));
  }
  i = nPts + nGCs;
  h = 2.0 / (x(i) - x(i-1));
  gradL(i) = h * (factor1 * (values(i) - values(i-1)) -
		  factor2 * (values(i+1) - values(i-2)));
  gradR(i) = (values(i+1) - values(i)) / (x(i+1) - x(i));
  
  gradients = limiter_mc(gradL, gradR, nPts, nGCs);

  return gradients;
}

// ---------------------------------------------------------
// Project gradients + values to the right face, from the left
//   returned values are on the i - 1/2 edges
//     (between i-1 and i cell center)
// ---------------------------------------------------------

arma_vec project_from_left(arma_vec values,
			   arma_vec gradients,
			   arma_vec x_centers,
			   arma_vec x_edges,
			   int64_t nPts,
			   int64_t nGCs) {
  int64_t iStart = 0;
  int64_t iEnd = nPts + 2 * nGCs;

  // Define at edges:
  arma_vec projected(nPts + 2 * nGCs + 1);
  projected.zeros();

  // no gradient in the 0 or iEnd cells
  for (int64_t i = iStart + 1; i < iEnd - 1; i++)
    projected(i + 1) = values(i) +
      gradients(i) * (x_edges(i + 1) - x_centers(i));

  return projected;
}


// ---------------------------------------------------------
// Project gradients + values to the left face, from the right
//   returned values are on the i - 1 edges
//     (between i-1 and i cell center)
// ---------------------------------------------------------

arma_vec project_from_right(arma_vec values,
			    arma_vec gradients,
			    arma_vec x_centers,
			    arma_vec x_edges,
			    int64_t nPts,
			    int64_t nGCs) {
  int64_t iStart = 0;
  int64_t iEnd = nPts + 2 * nGCs;

  // Define at edges:
  arma_vec projected(nPts + 2 * nGCs + 1);
  projected.zeros();

  // no gradient in the 0 or iEnd cells
  for (int64_t i = iStart + 1; i < iEnd - 1; i++)
    projected(i) = values(i) +
      gradients(i) * (x_edges(i) - x_centers(i));

  return projected;
}

// ---------------------------------------------------------
// Limiter on values
//   projected is assumed to be on the edge between the
//   i-1 and i cell (i-1/2)
//   limited is returned at edges
// ---------------------------------------------------------

arma_vec limiter_value(arma_vec projected,
		       arma_vec values,
		       int64_t nPts,
		       int64_t nGCs) {
  
  int64_t iStart = 0;
  int64_t iEnd = nPts + 2 * nGCs;

  arma_vec limited = projected;

  precision_t mini, maxi;
  
  for (int64_t i = iStart + 1; i < iEnd - 1; i++) {

    mini = values(i-1);
    if (values(i) < mini)
      mini = values(i);
    maxi = values(i-1);
    if (values(i) > maxi)
      maxi = values(i);

    if (limited(i) < mini)
      limited(i) = mini;
    if (limited(i) > maxi)
      limited(i) = maxi;
    
  }
  return limited;
}


// ---------------------------------------------------------
// Project gradients + values to the right face, from the left
//   returned values are on the i - 1/2 edges
//     (between i-1 and i cell center)
// ---------------------------------------------------------

arma_vec project_from_left_new(arma_vec values,
			       arma_vec x_centers,
			       arma_vec x_edges,
			       int64_t nPts,
			       int64_t nGCs) {
  int64_t iStart = 1;
  int64_t iEnd = nPts + 2 * nGCs - 1;

  // Define at edges:
  arma_vec projected(nPts + 2 * nGCs + 1);
  projected.zeros();

  precision_t dxei, dxci, dxcip1, r;
  
  // no gradient in the 0 or iEnd cells
  for (int64_t i = iStart; i < iEnd; i++) {
    dxei = x_edges(i + 1) - x_edges(i);
    dxci = x_centers(i) - x_centers(i - 1);
    dxcip1 = x_centers(i + 1) - x_centers(i);
    r = dxcip1 / dxci;
    projected(i + 1) = values(i) + 
      0.5 * dxei * (values(i) - values(i - 1)) / dxci + 
      0.125 * dxei * dxei * (values(i + 1) + r * values(i - 1) - (1 + r) * values(i)) / (dxci * dxcip1);
  }

  projected = limiter_value(projected, values, nPts, nGCs);
  
  return projected;
}

// ---------------------------------------------------------
// Project gradients + values to the left face, from the right
//   returned values are on the i - 1 edges
//     (between i-1 and i cell center)
// ---------------------------------------------------------

arma_vec project_from_right_new(arma_vec values,
				arma_vec x_centers,
				arma_vec x_edges,
				int64_t nPts,
				int64_t nGCs) {
  int64_t iStart = 1;
  int64_t iEnd = nPts + 2 * nGCs - 1;

  // Define at edges:
  arma_vec projected(nPts + 2 * nGCs + 1);
  precision_t dxei, dxci, dxcip1, r;
  
  projected.zeros();

  // no gradient in the 0 or iEnd cells
  for (int64_t i = iStart; i < iEnd; i++) {
    dxei = x_edges(i + 1) - x_edges(i);
    dxci = x_centers(i) - x_centers(i - 1);
    dxcip1 = x_centers(i + 1) - x_centers(i);
    r = dxcip1 / dxci;
    projected(i) = values(i) - 
      0.5 * dxei * (values(i + 1) - values(i)) / dxcip1 +
      0.125 * dxei * dxei * (values(i + 1) + r * values(i - 1) - (1 + r) * values(i)) / (dxci * dxcip1);
  }

  projected = limiter_value(projected, values, nPts, nGCs);
  
  return projected;
}

// ---------------------------------------------------------
// gudonov upwind scheme
// ---------------------------------------------------------

arma_vec gudonov(arma_vec valL,
		 arma_vec valR,
		 arma_vec velL,
		 arma_vec velR,
		 int64_t nPts,
		 int64_t nGCs) {
  
  int64_t iStart = 0;
  int64_t iEnd = nPts + 2 * nGCs;

  arma_vec flux = velL * 0.0;
  arma_vec vel = (velL + velR)/2.0;
  
  for (int64_t i = iStart + 1; i < iEnd - 1; i++) {
    if (vel(i) > 0)
      flux(i) = valR(i) * vel(i);
    else
      flux(i) = valL(i) * vel(i);
  }
  return flux;
}

// ---------------------------------------------------------
// gudonov upwind scheme
// ---------------------------------------------------------

arma_vec rusanov(arma_vec valL,
		 arma_vec valR,
		 arma_vec velL,
		 arma_vec velR,
		 arma_vec widths,
		 int64_t nPts,
		 int64_t nGCs) {
  
  int64_t iStart = 0;
  int64_t iEnd = nPts + 2 * nGCs;

  arma_vec ws = abs((velL + velR)/2.0) + 1.;
  arma_vec fluxL = valL % velL;
  arma_vec fluxR = valR % velR;
  arma_vec valDiff = valL - valR;
  arma_vec flux = (fluxL + fluxR) / 2.0;
  for (int64_t i = iStart + 1; i < iEnd - 1; i++)
    flux(i) = flux(i) - ws(i)/2 * valDiff(i);
    
  return flux;
}

// ---------------------------------------------------------
//
// ---------------------------------------------------------

void output(arma_vec values,
	    std::string filename,
	    bool DoAppend,
	    int64_t nPts,
	    int64_t nGCs) {

  std::ofstream outfile;
  if (DoAppend)
    outfile.open(filename, std::ios_base::app);
  else
    outfile.open(filename);
  
  int64_t i;
  for (i = 0; i < nPts + 2 * nGCs ; i++) {
    outfile << values(i) << " ";
  }
  outfile << "\n";
  outfile.close();
    
}


// ---------------------------------------------------------
//
// ---------------------------------------------------------


// ---------------------------------------------------------
// main code
// ---------------------------------------------------------

int main() {

  precision_t timeMax = 0.1;
  precision_t time = 0.0;
  
  precision_t gamma = 5.0/3.0;
  precision_t KoM = kb/mass;
  //gamma = kb/mass;

  int64_t iStep;
  
  int64_t nPts = 100, i;
  int64_t nGCs = 2;
  int64_t nPtsTotal = nGCs + nPts + nGCs;

  arma_vec x = init_grid(nPts, nGCs);
  //arma_vec x = init_stretched_grid(nPts, nGCs);
  arma_vec edges = calc_bin_edges(x);
  arma_vec widths = calc_bin_widths(edges);

  precision_t dt = 0.00001 * x(nPts + nGCs - 1) / nPts;
  int64_t nSteps = 100.0 / dt;
  
  // std::cout << "dt : " << dt << "; nSteps: " << nSteps << "\n";

  // state variables:
  arma_vec rho = init_rho(nPtsTotal, x);
  arma_vec grad_rho;
  arma_vec rhoL;
  arma_vec rhoR;

  arma_vec vel = init_vel(nPtsTotal);
  arma_vec grad_vel;
  arma_vec velL;
  arma_vec velR;

  // temp is "e" (not E):
  arma_vec temp = init_temp(nPtsTotal);
  arma_vec grad_temp;
  arma_vec tempL, tempR;

  arma_vec eq1Flux, eq1FluxL, eq1FluxR;
  arma_vec eq2Flux, eq2FluxL, eq2FluxR;
  arma_vec eq3Flux, eq3FluxL, eq3FluxR;
  arma_vec wsL, wsR, ws;
  arma_vec dtAll;

  arma_vec diff;

//  exchange(rho, nPts, nGCs);
//  exchange(vel, nPts, nGCs);
//  exchange(temp, nPts, nGCs);

  arma_vec momentum = rho % vel;
  arma_vec grad_momenum, momentumL, momentumR;

  arma_vec totalE = rho % temp * KoM + 0.5 * rho % vel % vel;
  arma_vec grad_totalE, totaleL, totaleR;
  
  output(rho, "rho.txt", false, nPts, nGCs);
  output(vel, "vel.txt", false, nPts, nGCs);
  output(temp, "temp.txt", false, nPts, nGCs);
  output(totalE, "totale.txt", false, nPts, nGCs);
  output(x, "x.txt", false, nPts, nGCs);

  iStep = 0;
  while (time < timeMax) {

    std::cout << "iStep = " << iStep
	      << "; time =  " << time
	      << "; vel =  " << vel(80) << "\n";

    // -----------------------------------
    // Rho

    grad_rho = calc_grad(rho, x, nPts, nGCs);

    // Right side of edge from left
    rhoR = project_from_left_new(rho,
			     x, edges,
			     nPts, nGCs);
    
    // Left side of edge from left
    rhoL = project_from_right_new(rho,
			      x, edges,
			      nPts, nGCs);

    // -----------------------------------
    // vel

    grad_vel = calc_grad(vel, x, nPts, nGCs);
    // Right side of edge from left
    velR = project_from_left_new(vel,
			     x, edges,
			     nPts, nGCs);
    // Left side of edge from left
    velL = project_from_right_new(vel,
			      x, edges,
			      nPts, nGCs);

    // -----------------------------------
    // temp

    grad_temp = calc_grad(temp, x, nPts, nGCs);
    // Right side of edge from left
    tempR = project_from_left_new(temp,
			      x, edges,
			      nPts, nGCs);
    // Left side of edge from left
    tempL = project_from_right_new(temp,
			       x, edges,
			       nPts, nGCs);

    // eq 1 = rho
    // eq 2 = rho * vel (momentum)
    // eq 3 = E --> rho * (temp + 0.5 * vel^2) (totalE) 
    
    // Calculate fluxes of different terms at the edges:
    eq1FluxL = rhoL % velL;
    eq1FluxR = rhoR % velR;

    momentumL = eq1FluxL;
    momentumR = eq1FluxR;
    totaleL = rhoL % tempL * KoM + 0.5 * rhoL % velL % velL;
    totaleR = rhoR % tempR * KoM + 0.5 * rhoR % velR % velR;

    //eq2FluxL = rhoL % (velL % velL + (gamma-1) * tempL);
    //eq2FluxR = rhoR % (velR % velR + (gamma-1) * tempR);
    eq2FluxL = rhoL % (velL % velL + KoM * tempL);
    eq2FluxR = rhoR % (velR % velR + KoM * tempR);

    //eq3FluxL = rhoL % velL % (0.5 * velL % velL + gamma * tempL);
    //eq3FluxR = rhoR % velR % (0.5 * velR % velR + gamma * tempR);
    eq3FluxL = rhoL % velL % (0.5 * velL % velL + gamma * tempL * KoM);
    eq3FluxR = rhoR % velR % (0.5 * velR % velR + gamma * tempR * KoM);

    // Calculate the wave speed for the diffusive flux:
    //wsL = abs(velL) + sqrt(gamma * (gamma-1.0) * tempL);
    //wsR = abs(velR) + sqrt(gamma * (gamma-1.0) * tempR);
    wsL = abs(velL) + sqrt(gamma * KoM * tempL);
    wsR = abs(velR) + sqrt(gamma * KoM * tempR);
    ws = wsR;
    dt = 0.0;
    for (i = 1; i < nPts + nGCs*2 - 1; i++) {
      if (wsR(i) > ws(i)) ws(i) = wsR(i);
      if (widths(i) / ws(i) > dt)
	dt = widths(i) / ws(i);
    }
    dt = dt * 0.001;
    time = time + dt;
    
    // Calculate average flux at the edges:
    diff = rhoR - rhoL;
    eq1Flux = (eq1FluxL + eq1FluxR) / 2 + 0.5 * ws % diff;
    diff = momentumR - momentumL;
    eq2Flux = (eq2FluxL + eq2FluxR) / 2 + 0.5 * ws % diff;
    diff = totaleR - totaleL;
    eq3Flux = (eq3FluxL + eq3FluxR) / 2 + 0.5 * ws % diff;
    
    // Update values:
    for (i = nGCs; i < nPts + nGCs*2 - 1; i++) {
      rho(i) = rho(i) - dt / widths(i) * (eq1Flux(i+1) - eq1Flux(i));
      momentum(i) = momentum(i) - dt / widths(i) * (eq2Flux(i+1) - eq2Flux(i))
	+ gravity * rho(i) * dt;
      totalE(i) = totalE(i) - dt / widths(i) * (eq3Flux(i+1) - eq3Flux(i));
    }

    //exchange(rho, nPts, nGCs);
    //exchange(momentum, nPts, nGCs);
    //exchange(totalE, nPts, nGCs);
    vel = momentum / rho;
    temp = (totalE / rho - 0.5 * vel % vel) / KoM;
    
    set_bcs(nPts, nGCs, x, rho, vel, temp);

    output(rho, "rho.txt", true, nPts, nGCs);
    output(vel, "vel.txt", true, nPts, nGCs);
    output(temp, "temp.txt", true, nPts, nGCs);
    output(totalE, "totale.txt", true, nPts, nGCs);
    //output(rhoL, "rhor.txt", false, nPts, nGCs);
    //output(rhoR, "rhol.txt", false, nPts, nGCs);
    iStep++;
    
  }
  
  return 0;
}