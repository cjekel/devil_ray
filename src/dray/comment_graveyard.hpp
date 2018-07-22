
//// // ---------------------------------
//// // Support for Shape Type functions.
//// // ---------------------------------
//// 
//// // Tensor product of arrays. Input arrays start at starts[0], starts[1], ... and each has stride of Stride.
//// // (This makes array storage flexible: they can be stored separately, contiguously, or interleaved.)
//// // The layout of the output array is such that the last given index is iterated first (innermost, stride of 1),
//// // and the first given index is iterated last (outermost, stride of el_dofs_1d ^ (RefDim-1)).
//// template <typename T, int32 RefDim, int32 InStride, int32 OutStride>
//// struct TensorProduct
//// {
////   // Computes and stores a single component of the tensor.
////   DRAY_EXEC
////   void operator() (int32 el_dofs_1d, const T* starts[], int32 out_idx, T* out) const
////   {
////     //int32 out_stride = 1;
////     //int32 out_idx = 0;
////     T out_val = 1;
////     for (int32 rdim = RefDim-1, idx_mask_right = 1; rdim >= 0; rdim--, idx_mask_right *= el_dofs_1d)
////     {
////       //int32 dim_idx = idx[rdim];
////       int32 dim_idx = (out_idx / idx_mask_right) % el_dofs_1d;
////       //out_idx += dim_idx * dim_stride;
////       out_val *= starts[rdim][dim_idx * InStride];
////     }
////     out[out_idx * OutStride] = out_val;
////   }
//// };
//// 


//// 
//// // --- Shape Type Public Interface --- //
//// //
//// // int32 get_el_dofs() const;
//// // int32 get_ref_dim() const;
//// // void calc_shape_dshape(const Array<int> &active_idx, const Array<Vec<RefDim>> &ref_pts, Array<T> &shape_val, Array<Vec<RefDim>> &shape_deriv) const; 
//// //
//// //
//// // --- Internal Parameters (example) --- //
//// //
//// // static constexpr int32 ref_dim;
//// // int32 p_order;
//// // int32 el_dofs;
//// 
//// // Abstract TensorShape class defines mechanics of tensor product.
//// // Inherit from this and define 2 methods:
//// // 1. Class method get_el_dofs_1d();
//// // 2. External function in the (template parameter) class Shape1D, calc_shape_dshape_1d();
//// template <typename T, int32 RefDim, typename Shape1D>
//// struct TensorShape
//// {
////   virtual int32 get_el_dofs_1d() const = 0;
//// 
////   int32 get_ref_dim() const { return RefDim; }
////   int32 get_el_dofs() const { return pow(get_el_dofs_1d(), RefDim); }
//// 
////   void calc_shape_dshape( const Array<int32> &active_idx,
////                           const Array<Vec<T,RefDim>> &ref_pts,
////                           Array<T> &shape_val,                      // Will be resized.
////                           Array<Vec<T,RefDim>> &shape_deriv) const;   // Will be resized.
//// };
//// 
//// 
//// template <typename T>
//// struct Bernstein1D
//// {
////   // Bernstein evaluator rippped out of MFEM.
////   DRAY_EXEC
////   static void calc_shape_dshape_1d(int32 el_dofs_1d, const T x, const T y, T *u, T *d)
////   {
////     const int32 p = el_dofs_1d - 1;
////     if (p == 0)
////     {
////        u[0] = 1.;
////        d[0] = 0.;
////     }
////     else
////     {
////        // Write binomial coefficients into u memory instead of allocating b[].
////        BinomRow<T>::fill_single_row(p,u);
//// 
////        const double xpy = x + y, ptx = p*x;
////        double z = 1.;
//// 
////        int i;
////        for (i = 1; i < p; i++)
////        {
////           //d[i] = b[i]*z*(i*xpy - ptx);
////           d[i] = u[i]*z*(i*xpy - ptx);
////           z *= x;
////           //u[i] = b[i]*z;
////           u[i] = u[i]*z;
////        }
////        d[p] = p*z;
////        u[p] = z*x;
////        z = 1.;
////        for (i--; i > 0; i--)
////        {
////           d[i] *= z;
////           z *= y;
////           u[i] *= z;
////        }
////        d[0] = -p*z;
////        u[0] = z*y;
////     }
////   }
//// 
////   DRAY_EXEC static bool IsInside(const T ref_coord)
////   {
////     //TODO some tolerance?  Where can we make watertight?
////     // e.g. Look at MFEM's Geometry::CheckPoint(geom, ip, ip_tol)
////     return 0.0 <= ref_coord  &&  ref_coord < 1.0;
////   }
//// };
//// 
//// template <typename T, int32 RefDim>
//// struct BernsteinShape : public TensorShape<T, RefDim, Bernstein1D<T>>
//// {
////   int32 m_p_order;
//// 
////   virtual int32 get_el_dofs_1d() const { return m_p_order + 1; }
//// 
////   // Inherits from TensorShape
////   // - int32 get_ref_dim() const;
////   // - int32 get_el_dofs() const;
////   // - void calc_shape_dshape(...) const;
//// 
////   DRAY_EXEC static bool IsInside(const Vec<T,RefDim> ref_pt)
////   {
////     for (int32 rdim = 0; rdim < RefDim; rdim++)
////     {
////       if (!Bernstein1D<T>::IsInside(ref_pt[rdim])) return false;
////     };
////     return true;
////   }
//// 
////   static BernsteinShape factory(int32 p)
////   {
////     BernsteinShape ret;
////     ret.m_p_order = p;
////     return ret;
////   }
//// };


//
// ShapeOp Interface
//
//--//   template <typename T, int32 RefDim>
//--//   struct ShapeOp
//--//   {
//--//     static constexpr int32 ref_dim = RefDim;
//--//
//--//     int32 get_el_dofs() const;
//--//   
//--//     // Stateful operator, where state includes polynomial order, pointer to auxiliary memory, etc.
//--//     T *m_aux_mem_ptr;
//--//     void set_aux_mem_ptr(T *aux_mem_ptr) { m_aux_mem_ptr = aux_mem_ptr; }
//--//
//--//     // The number of auxiliary elements needed for linear_combo() parameter aux_mem.
//--//     int32 get_aux_req() const;
//--//     bool static is_aux_req();
//--//   
//--//     template <typename CoeffIterType, int32 PhysDim>
//--//     DRAY_EXEC void linear_combo(const Vec<T,RefDim> &xyz,
//--//                                 const CoeffIterType &coeff_iter,
//--//                                 Vec<PhysDim> &result_val,
//--//                                 Vec<Vec<T,PhysDim>,RefDim> &result_deriv);
//--//
//--//     // If just want raw shape values/derivatives,
//--//     // stored in memory, to do something with them later:
//--//     DRAY_EXEC void calc_shape_dshape(const Vec<RefDim> &ref_pt, T *shape_val, Vec<RefDim> *shape_deriv) const;   //Optional
//--//   };


//
// BernsteinBasis - ShapeOp w/ respect to Bernstein basis functions in arbitrary number of reference dimensions.
//

////// // The Idea.
///////DRAY_EXEC static void linear_combo_power_basis(
///////    const int32 p,
///////    const T x,
///////    const T y,
///////    const Vec<T,PhysDim> *coeff,
///////    Vec<T,PhysDim> &ac_v,
///////    Vec<T,PhysDim> &ac_dx,
///////    Vec<T,PhysDim> &ac_dy)
///////{
///////  ac_v = 0;
///////  ac_dx = 0;
///////  ac_dy = 0;
///////  Vec<T,PhysDim> ac_v_i;   // "inner"
///////  Vec<T,PhysDim> ac_dy_i;  // "inner"
///////  for (int32 k = p; k > 0; k--)
///////  {
///////    linear_combo_power_basis(p, y, coeff + k * (p+1), ac_v_i, ac_dy_i);
///////    ac_v = ac_v * x + ac_v_i;
///////    ac_dy = ac_dy * x + ac_dy_i;
///////    ac_dx = ac_dx * x + ac_v_i * k;
///////  }
///////  linear_combo_power_basis(p, y, coeff + k * (p+1), ac_v_i, ac_dy_i);
///////  ac_v = ac_v * x + ac_v_i;
///////  ac_dy = ac_dy * x + ac_dy_i;
///////}


//
//
// Notes
//
//

//---- Interface ----//
//
//--//   template <typename T, int32 RefDim>
//--//   struct ShapeOp
//--//   {
//--//     static constexpr int32 ref_dim = RefDim;
//--//   
//--//     // Stateful operator, where state includes polynomial order, pointer to auxiliary memory, etc.
//--//     T *m_aux_mem_ptr;
//--//     void set_aux_mem_ptr(T *aux_mem_ptr) { m_aux_mem_ptr = aux_mem_ptr; }
//--//
//--//     // The number of auxiliary elements needed for linear_combo() parameter aux_mem.
//--//     int32 get_aux_req() const;
//--//     bool static is_aux_req();
//--//   
//--//     template <typename CoeffIterType>
//--//     DRAY_EXEC void linear_combo(const Vec<T,RefDim> &xyz, const CoeffIterType &coeff_iter,
//--//                                   Vec<CoeffIterType::phys_dim> &result_val, Vec<Vec<T,CoeffIterType::phys_dim>,RefDim> &result_deriv);
//--//   };
