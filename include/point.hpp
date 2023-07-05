
#ifndef __UTILS_GEOMETRY_HPP__
#define __UTILS_GEOMETRY_HPP__

#include <concepts>

template<typename T>
concept IsSignedNumber =  std::floating_point<T> || std::signed_integral<T>;

template<IsSignedNumber T>
struct point2d_base {
	T x;
	T y;

    using point_type = T;

	constexpr point2d_base& operator+=(const point2d_base& pt) {
		x += pt.x;
		y += pt.y;
		return *this;
	}

	constexpr point2d_base& operator-() {
		x = -x;
		y = -y;
		return *this;
	}

	constexpr point2d_base& operator-=(const point2d_base& pt) {
		x -= pt.x;
		y -= pt.y;
		return *this;
	}

	constexpr point2d_base& operator*=(auto val) {
		x *= val;
		y *= val;
		return *this;
	}

	constexpr point2d_base& operator/=(auto val) {
		x /= val;
		y /= val;
		return *this;
	}
};

template<typename T>
inline constexpr point2d_base<T> operator+(point2d_base<T> lhp, const point2d_base<T>& rhp) {
	lhp += rhp;
	return lhp;
}

template<typename T>
inline constexpr point2d_base<T> operator-(point2d_base<T> lhp, const point2d_base<T>& rhp) {
	lhp -= rhp;
	return lhp;
}

template<typename T>
inline constexpr point2d_base<T> operator*(point2d_base<T> pt, auto val) {
	pt *= val;
	return pt;
}

template<typename T>
inline constexpr point2d_base<T> operator*(auto val, point2d_base<T> pt) {
	return pt * val;
}

template<typename T>
inline constexpr point2d_base<T> operator/(point2d_base<T> pt, auto val) {
	pt /= val;
	return pt;
}

template<IsSignedNumber T>
struct point3d_base {
	T x;
	T y;
	T z;

    using point_type = T;

	constexpr point3d_base& operator+=(const point3d_base& pt) {
		x += pt.x;
		y += pt.y;
		z += pt.z;
		return *this;
	}

	constexpr point3d_base& operator-() {
		x = -x;
		y = -y;
		z = -z;
		return *this;
	}

	constexpr point3d_base& operator-=(const point3d_base& pt) {
		x -= pt.x;
		y -= pt.y;
		z -= pt.z;
		return *this;
	}

	constexpr point3d_base& operator*=(auto val) {
		x *= val;
		y *= val;
		z *= val;
		return *this;
	}

	constexpr point3d_base& operator/=(auto val) {
		x /= val;
		y /= val;
		z /= val;
		return *this;
	}
};

template<typename T>
inline constexpr point3d_base<T> operator+(point3d_base<T> lhp, const point3d_base<T>& rhp) {
	lhp += rhp;
	return lhp;
}

template<typename T>
inline constexpr point3d_base<T> operator-(point3d_base<T> lhp, const point3d_base<T>& rhp) {
	lhp -= rhp;
	return lhp;
}

template<typename T>
inline constexpr point3d_base<T> operator*(point3d_base<T> pt, auto val) {
	pt *= val;
	return pt;
}

template<typename T>
inline constexpr point3d_base<T> operator*(auto val, point3d_base<T> pt) {
	return pt * val;
}

template<typename T>
inline constexpr point3d_base<T> operator/(point3d_base<T> pt, auto val) {
	pt /= val;
	return pt;
}


using point2d  = point2d_base<float>;
using point2di = point2d_base<int>;
using point3d  = point3d_base<float>;
using point3di = point3d_base<int>;

#endif // __UTILS_GEOMETRY_HPP__
