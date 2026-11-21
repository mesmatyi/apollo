# Reference Line Smoothing Algorithms

_**Tip**: To better display the equations in this document, we recommend using the Chrome browser with the [plugin](https://chrome.google.com/webstore/detail/tex-all-the-things/cbimabofgmfdkicghcadidpemeenbffn), or copying the LaTeX equations into an [online formula editing site](http://www.hostmath.com/) to view them._

Quadratic Programming (QP) + spline interpolation

## 1. Objective function

### 1.1 Segmenting the routing path

Divide the routing path into **n** segments, each represented by 2 polynomials:

```
$$
x = f_i(t)
  = a_{i0} + a_{i1} * t + a_{i2} * t^2 + a_{i3} * t^3 + a_{i4} * t^4 + a_{i5} * t^5
$$
```

```
$$
y = g_i(t) = b_{i0} + b_{i1} * t + b_{i2} * t^2 + b_{i3} * t^3 + b_{i4} * t^4 + b_{i5} * t^5
$$
```

### 1.2 Defining the spline segment optimization objective function

```
$$
cost =
\sum_{i=1}^{n}
\Big(
\int\limits_{0}^{t_i} (f_i''')^2(t) dt
+ \int\limits_{0}^{t_i} (g_i''')^2(t) dt
\Big)
$$
```

### 1.3 Converting the cost function into QP form

QP formulation:

```
$$
\frac{1}{2} \cdot x^T \cdot H \cdot x + f^T \cdot x
\\
s.t. LB \leq x \leq UB
\\
A_{eq}x = b_{eq}
\\
Ax \leq b
$$
```

## 2 Constraints

### 2.1 Smooth knot constraints

The purpose of this constraint is to make the knots of the spline smoother. Suppose two segments $seg_k$ and $seg_{k+1}$ are connected, and the accumulated value **s** of $seg_k$ is $s_k$. The equation for computing the constraint is:

```
$$
f_k(s_k) = f_{k+1} (s_0)
$$
```

Similarly, the same formulation applies to the following equations:

```
$$
f'_k(s_k) = f'_{k+1} (s_0)
\\
f''_k(s_k) = f''_{k+1} (s_0)
\\
f'''_k(s_k) = f'''_{k+1} (s_0)
\\
g_k(s_k) = g_{k+1} (s_0)
\\
g'_k(s_k) = g'_{k+1} (s_0)
\\
g''_k(s_k) = g''_{k+1} (s_0)
\\
g'''_k(s_k) = g'''_{k+1} (s_0)
$$
```

### 2.2 Point sampling boundary constraints

Uniformly sample **m** points on the path and check the predefined boundaries of these points.

```
$$
f_i(t_l) - x_l< boundary
\\
g_i(t_l) - y_l< boundary
$$
```
