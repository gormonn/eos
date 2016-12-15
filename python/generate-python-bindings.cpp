/*
 * eos - A 3D Morphable Model fitting library written in modern C++11/14.
 *
 * File: python/generate-python-bindings.cpp
 *
 * Copyright 2016 Patrik Huber
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "eos/core/LandmarkMapper.hpp"
#include "eos/morphablemodel/PcaModel.hpp"
#include "eos/morphablemodel/MorphableModel.hpp"
#include "eos/morphablemodel/Blendshape.hpp"
#include "eos/morphablemodel/EdgeTopology.hpp"
#include "eos/fitting/contour_correspondence.hpp"
#include "eos/fitting/fitting.hpp"
#include "eos/fitting/orthographic_camera_estimation_linear.hpp"
#include "eos/fitting/RenderingParameters.hpp"
#include "eos/render/Mesh.hpp"

#include "opencv2/core/core.hpp"

#include "pybind11/pybind11.h"
#include "pybind11/stl.h"
#include "pybind11_glm.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <algorithm>
#include <cassert>

namespace py = pybind11;
using namespace eos;

/**
 * Generate python bindings for the eos library using pybind11.
 */
PYBIND11_PLUGIN(eos) {
    py::module eos_module("eos", "Python bindings for the eos 3D Morphable Face Model fitting library.\n\nFor an overview of the functionality, see the documentation of the submodules. For the full documentation, see the C++ doxygen documentation.");

	/**
	 * General bindings, for OpenCV vector types and cv::Mat:
	 *  - cv::Vec2f
	 *  - cv::Vec4f
	 *  - cv::Mat (only 1-channel matrices and only conversion of CV_32F C++ matrices to Python, and conversion of CV_32FC1 and CV_64FC1 matrices from Python to C++)
	 */
	py::class_<cv::Vec2f>(eos_module, "Vec2f", "Wrapper for OpenCV's cv::Vec2f type.")
		.def("__init__", [](cv::Vec2f& vec, py::buffer b) {
			py::buffer_info info = b.request();

			if (info.ndim != 1)
				throw std::runtime_error("Buffer ndim is " + std::to_string(info.ndim) + ", please hand a buffer with dimension == 1 to create a Vec2f.");
			if (info.strides.size() != 1)
				throw std::runtime_error("strides.size() is " + std::to_string(info.strides.size()) + ", please hand a buffer with strides.size() == 1 to create a Vec2f.");
			// Todo: Should add a check that checks for default stride sizes, everything else would not work yet I think.
			if (info.shape.size() != 1)
				throw std::runtime_error("shape.size() is " + std::to_string(info.shape.size()) + ", please hand a buffer with shape dimension == 1 to create a Vec2f.");
			if (info.shape[0] != 2)
				throw std::runtime_error("shape[0] is " + std::to_string(info.shape[0]) + ", please hand a buffer with 2 entries to create a Vec2f.");

			if (info.format == py::format_descriptor<float>::format())
			{
				cv::Mat temp(1, 2, CV_32FC1, info.ptr);
				//std::cout << temp << std::endl;
				new (&vec) cv::Vec2f(temp);
			}
			else {
				throw std::runtime_error("Not given a buffer of type float - please hand a buffer of type float to create a Vec2f.");
			}
		})
		.def_buffer([](cv::Vec2f& vec) -> py::buffer_info {
		return py::buffer_info(
			&vec.val,                               /* Pointer to buffer */
			sizeof(float),                          /* Size of one scalar */
			py::format_descriptor<float>::format(), /* Python struct-style format descriptor */
			2,                                      /* Number of dimensions */
			{ vec.rows, vec.cols },                 /* Buffer dimensions */
			{ sizeof(float),             /* Strides (in bytes) for each index */
			sizeof(float) }					/* => both sizeof(float), since the data is hold in an array, i.e. contiguous memory */
		);
	});

	py::class_<cv::Vec4f>(eos_module, "Vec4f", "Wrapper for OpenCV's cv::Vec4f type.")
		.def("__init__", [](cv::Vec4f& vec, py::buffer b) {
			py::buffer_info info = b.request();

			if (info.ndim != 1)
				throw std::runtime_error("Buffer ndim is " + std::to_string(info.ndim) + ", please hand a buffer with dimension == 1 to create a Vec4f.");
			if (info.strides.size() != 1)
				throw std::runtime_error("strides.size() is " + std::to_string(info.strides.size()) + ", please hand a buffer with strides.size() == 1 to create a Vec4f.");
			// Todo: Should add a check that checks for default stride sizes, everything else would not work yet I think.
			if (info.shape.size() != 1)
				throw std::runtime_error("shape.size() is " + std::to_string(info.shape.size()) + ", please hand a buffer with shape dimension == 1 to create a Vec4f.");
			if (info.shape[0] != 4)
				throw std::runtime_error("shape[0] is " + std::to_string(info.shape[0]) + ", please hand a buffer with 4 entries to create a Vec4f.");

			if (info.format == py::format_descriptor<float>::format())
			{
				cv::Mat temp(1, 4, CV_32FC1, info.ptr);
				//std::cout << temp << std::endl;
				new (&vec) cv::Vec4f(temp);
			}
			else {
				throw std::runtime_error("Not given a buffer of type float - please hand a buffer of type float to create a Vec4f.");
			}
		})
		.def_buffer([](cv::Vec4f& vec) -> py::buffer_info {
		return py::buffer_info(
			&vec.val,                               /* Pointer to buffer */
			sizeof(float),                          /* Size of one scalar */
			py::format_descriptor<float>::format(), /* Python struct-style format descriptor */
			2,                                      /* Number of dimensions */
			{ vec.rows, vec.cols },                 /* Buffer dimensions */
			{ sizeof(float),             /* Strides (in bytes) for each index */
			sizeof(float) }					/* => both sizeof(float), since the data is hold in an array, i.e. contiguous memory */
		);
	});

	py::class_<cv::Mat>(eos_module, "Mat", "Wrapper for OpenCV's cv::Mat type (currently only 1-channel matrices are supported and only conversion of CV_32F C++ matrices to Python, and conversion of CV_32FC1 and CV_64FC1 matrices from Python to C++).")
		// This adds support for creating eos.Mat objects in Python from buffers like NumPy arrays:
		.def("__init__", [](cv::Mat& mat, py::buffer b) {
			py::buffer_info info = b.request();
			
			if (info.ndim != 2)
				throw std::runtime_error("Buffer ndim is " + std::to_string(info.ndim) + ", only buffer dimension == 2 is currently supported.");
			if (info.strides.size() != 2)
				throw std::runtime_error("strides.size() is " + std::to_string(info.strides.size()) + ", only strides.size() == 2 is currently supported.");
			// Todo: Should add a check that checks for default stride sizes, everything else would not work yet I think.
			if (info.shape.size() != 2)
				throw std::runtime_error("shape.size() is " + std::to_string(info.shape.size()) + ", only shape dimensions of == 2 are currently supported - i.e. only 2-dimensional matrices with rows and colums.");

			if (info.format == py::format_descriptor<float>::format())
			{
				new (&mat) cv::Mat(info.shape[0], info.shape[1], CV_32FC1, info.ptr); // uses AUTO_STEP
			}
			else if (info.format == py::format_descriptor<double>::format())
			{
				new (&mat) cv::Mat(info.shape[0], info.shape[1], CV_64FC1, info.ptr); // uses AUTO_STEP
			}
			else {
				throw std::runtime_error("Only the cv::Mat types CV_32FC1 and CV_64FC1 are currently supported. If needed, it should not be too hard to add other types.");
			}
		})
		// This gives cv::Mat a Python buffer interface, so the data can be used as NumPy array in Python:
		.def_buffer([](cv::Mat& mat) -> py::buffer_info {
			// Note: Exceptions within def_buffer don't seem to be shown in Python, use cout for now.
			if (!mat.isContinuous())
			{
				std::string error_msg("Only continuous (contiguous) cv::Mat objects are currently supported.");
				std::cout << error_msg << std::endl;
				throw std::runtime_error(error_msg);
			}
			// Note: Also stride/step should be 1 too, but I think this is covered by isContinuous().
			auto dimensions = mat.dims;
			if (dimensions != 2)
			{
				std::string error_msg("Only cv::Mat objects with dims == 2 are currently supported.");
				std::cout << error_msg << std::endl;
				throw std::runtime_error(error_msg);
			}
			if (mat.channels() != 1)
			{
				std::string error_msg("Only cv::Mat objects with channels() == 1 are currently supported.");
				std::cout << error_msg << std::endl;
				throw std::runtime_error(error_msg);
			}

			std::size_t rows = mat.rows;
			std::size_t cols = mat.cols;

			if (mat.type() == CV_32F) {
				return py::buffer_info(
					mat.data,                               /* Pointer to buffer */
					sizeof(float),                          /* Size of one scalar */
					py::format_descriptor<float>::format(), /* Python struct-style format descriptor */
					dimensions,                                      /* Number of dimensions */
					{ rows, cols },                 /* Buffer dimensions */
					{ sizeof(float) * cols,             /* Strides (in bytes) for each index */
					sizeof(float) }	// this way is correct for row-major memory layout (OpenCV)
				);
			}
			else {
				std::string error_msg("Only the cv::Mat type CV_32F is currently supported. If needed, it would be easy to add CV_8U and CV_64F.");
				std::cout << error_msg << std::endl;
				throw std::runtime_error(error_msg);
			}
			// Will never reach here.
		})
	;

	/**
	 * Bindings for the eos::core namespace:
	 *  - LandmarkMapper
	 */
	py::module core_module = eos_module.def_submodule("core", "Essential functions and classes to work with 3D face models and landmarks.");
	py::class_<core::LandmarkMapper>(core_module, "LandmarkMapper", "Represents a mapping from one kind of landmarks to a different format(e.g.model vertices).")
		.def(py::init<>(), "Constructs a new landmark mapper that performs an identity mapping, that is, its output is the same as the input.")
		.def("__init__", [](core::LandmarkMapper& instance, std::string filename) { // wrap the fs::path c'tor with std::string
				new (&instance) core::LandmarkMapper(filename);
			}, "Constructs a new landmark mapper from a file containing mappings from one set of landmark identifiers to another.")
		// We can't expose the convert member function yet - need std::optional (or some trick with self/this and a lambda)
		;

	/**
	 * Bindings for the eos::morphablemodel namespace:
	 *  - PcaModel
	 *  - MorphableModel
	 *  - load_model()
	 */
	py::module morphablemodel_module = eos_module.def_submodule("morphablemodel", "Functionality to represent a Morphable Model, its PCA models, and functions to load models and blendshapes.");

	py::class_<morphablemodel::PcaModel>(morphablemodel_module, "PcaModel", "Class representing a PcaModel with a mean, eigenvectors and eigenvalues, as well as a list of triangles to build a mesh.")
		.def("get_num_principal_components", &morphablemodel::PcaModel::get_num_principal_components, "Returns the number of principal components in the model.")
		.def("get_data_dimension", &morphablemodel::PcaModel::get_data_dimension, "Returns the dimension of the data, i.e. the number of shape dimensions.")
		.def("get_triangle_list", &morphablemodel::PcaModel::get_triangle_list, "Returns a list of triangles on how to assemble the vertices into a mesh.")
		.def("get_mean", &morphablemodel::PcaModel::get_mean, "Returns the mean of the model.")
		.def("get_mean_at_point", &morphablemodel::PcaModel::get_mean_at_point, "Return the value of the mean at a given vertex index.", py::arg("vertex_index"))
		.def("draw_sample", (cv::Mat (morphablemodel::PcaModel::*)(std::vector<float>) const)&morphablemodel::PcaModel::draw_sample, "Returns a sample from the model with the given PCA coefficients. The given coefficients should follow a standard normal distribution, i.e. not be scaled with their eigenvalues/variances.", py::arg("coefficients"))
		;

	py::class_<morphablemodel::MorphableModel>(morphablemodel_module, "MorphableModel", "A class representing a 3D Morphable Model, consisting of a shape- and colour (albedo) PCA model, as well as texture (uv) coordinates.")
		.def("get_shape_model", [](const morphablemodel::MorphableModel& m) { return m.get_shape_model(); }, "Returns the PCA shape model of this Morphable Model.") // Not sure if that'll really be const in Python? I think Python does a copy each time this gets called?
		.def("get_color_model", [](const morphablemodel::MorphableModel& m) { return m.get_color_model(); }, "Returns the PCA colour (albedo) model of this Morphable Model.")
		;

	morphablemodel_module.def("load_model", &morphablemodel::load_model, "Load a Morphable Model from a cereal::BinaryInputArchive (.bin) from the harddisk.");

	/**
	 *  - Blendshape
	 *  - load_blendshapes()
	 */
	py::class_<morphablemodel::Blendshape>(morphablemodel_module, "Blendshape", "A class representing a 3D blendshape.")
		.def_readwrite("name", &morphablemodel::Blendshape::name, "Name of the blendshape.")
		.def_readwrite("deformation", &morphablemodel::Blendshape::deformation, "A 3m x 1 col-vector (xyzxyz...)', where m is the number of model-vertices. Has the same format as PcaModel::mean.")
		;

	morphablemodel_module.def("load_blendshapes", &morphablemodel::load_blendshapes, "Load a file with blendshapes from a cereal::BinaryInputArchive (.bin) from the harddisk.");

	/**
	 *  - EdgeTopology
	 *  - load_edge_topology()
	 */
	py::class_<morphablemodel::EdgeTopology>(morphablemodel_module, "EdgeTopology", "A struct containing a 3D shape model's edge topology.");

	morphablemodel_module.def("load_edge_topology", &morphablemodel::load_edge_topology, "Load a 3DMM edge topology file from a json file.");

	/**
	 * Bindings for the eos::render namespace:
	 * (Note: Defining Mesh before using it below in fitting::fit_shape_and_pose)
	 *  - Mesh
	 */
	py::module render_module = eos_module.def_submodule("render", "3D mesh and texture extraction functionality.");

	py::class_<render::Mesh>(render_module, "Mesh", "This class represents a 3D mesh consisting of vertices, vertex colour information and texture coordinates.")
		.def_readwrite("vertices", &render::Mesh::vertices, "Vertices")
		.def_readwrite("tvi", &render::Mesh::tvi, "Triangle vertex indices")
		.def_readwrite("colors", &render::Mesh::colors, "Colour data")
		.def_readwrite("tci", &render::Mesh::tci, "Triangle colour indices (usually the same as tvi)")
		.def_readwrite("texcoords", &render::Mesh::texcoords, "Texture coordinates")
		;

	/**
	 * Bindings for the eos::fitting namespace:
	 *  - ScaledOrthoProjectionParameters
	 *  - RenderingParameters
	 *  - estimate_orthographic_projection_linear()
	 *  - ContourLandmarks
	 *  - ModelContour
	 *  - fit_shape_and_pose()
	 */
	py::module fitting_module = eos_module.def_submodule("fitting", "Pose and shape fitting of a 3D Morphable Model.");

	py::class_<fitting::ScaledOrthoProjectionParameters>(fitting_module, "ScaledOrthoProjectionParameters", "Parameters of an estimated scaled orthographic projection.")
		.def_readwrite("R", &fitting::ScaledOrthoProjectionParameters::R, "Rotation matrix")
		.def_readwrite("s", &fitting::ScaledOrthoProjectionParameters::s, "Scale")
		.def_readwrite("tx", &fitting::ScaledOrthoProjectionParameters::tx, "x translation")
		.def_readwrite("ty", &fitting::ScaledOrthoProjectionParameters::ty, "y translation")
		;

	py::class_<fitting::RenderingParameters>(fitting_module, "RenderingParameters", "Represents a set of estimated model parameters (rotation, translation) and camera parameters (viewing frustum).")
		.def(py::init<fitting::ScaledOrthoProjectionParameters, int, int>(), "Create a RenderingParameters object from an instance of estimated ScaledOrthoProjectionParameters.")
		.def("get_rotation", [](const fitting::RenderingParameters& p) { return glm::vec4(p.get_rotation().x, p.get_rotation().y, p.get_rotation().z, p.get_rotation().w); }, "Returns the rotation quaternion [x y z w].")
		.def("get_rotation_euler_angles", [](const fitting::RenderingParameters& p) { return glm::eulerAngles(p.get_rotation()); }, "Returns the rotation's Euler angles (in radians) as [pitch, yaw, roll].")
		.def("get_modelview", &fitting::RenderingParameters::get_modelview, "Returns the 4x4 model-view matrix.")
		.def("get_projection", &fitting::RenderingParameters::get_projection, "Returns the 4x4 projection matrix.")
		;

	fitting_module.def("estimate_orthographic_projection_linear", [](std::vector<glm::vec2> image_points, std::vector<glm::vec4> model_points, bool is_viewport_upsidedown, int viewport_height) {
			// We take glm vec's (transparent conversion in python) and convert them to OpenCV vec's for now:
			std::vector<cv::Vec2f> image_points_cv;
			std::for_each(std::begin(image_points), std::end(image_points), [&image_points_cv](auto&& p) { image_points_cv.push_back({p.x, p.y}); });
			std::vector<cv::Vec4f> model_points_cv;
			std::for_each(std::begin(model_points), std::end(model_points), [&model_points_cv](auto&& p) { model_points_cv.push_back({ p.x, p.y, p.z, p.w }); });
			const boost::optional<int> viewport_height_opt = viewport_height == 0 ? boost::none : boost::optional<int>(viewport_height);
			return fitting::estimate_orthographic_projection_linear(image_points_cv, model_points_cv, is_viewport_upsidedown, viewport_height_opt);
		}, "This algorithm estimates the parameters of a scaled orthographic projection, given a set of corresponding 2D-3D points.", py::arg("image_points"), py::arg("model_points"), py::arg("is_viewport_upsidedown"), py::arg("viewport_height") = 0)
		;

	py::class_<fitting::ContourLandmarks>(fitting_module, "ContourLandmarks", "Defines which 2D landmarks comprise the right and left face contour.")
		.def_static("load", &fitting::ContourLandmarks::load, "Helper method to load contour landmarks from a text file with landmark mappings, like ibug2did.txt.")
		;

	py::class_<fitting::ModelContour>(fitting_module, "ModelContour", "Definition of the vertex indices that define the right and left model contour.")
		.def_static("load", &fitting::ModelContour::load, "Helper method to load a ModelContour from a json file from the hard drive.")
		;
	
	fitting_module.def("fit_shape_and_pose", [](const morphablemodel::MorphableModel& morphable_model, const std::vector<morphablemodel::Blendshape>& blendshapes, const std::vector<glm::vec2>& landmarks, const std::vector<std::string>& landmark_ids, const core::LandmarkMapper& landmark_mapper, int image_width, int image_height, const morphablemodel::EdgeTopology& edge_topology, const fitting::ContourLandmarks& contour_landmarks, const fitting::ModelContour& model_contour, int num_iterations, int num_shape_coefficients_to_fit, float lambda) {
			assert(landmarks.size() == landmark_ids.size());
			std::vector<float> pca_coeffs;
			std::vector<float> blendshape_coeffs;
			std::vector<cv::Vec2f> fitted_image_points;
			// We can change this to std::optional as soon as we switch to VS2017 and pybind supports std::optional
			const boost::optional<int> num_shape_coefficients_opt = num_shape_coefficients_to_fit == -1 ? boost::none : boost::optional<int>(num_shape_coefficients_to_fit);
			core::LandmarkCollection<cv::Vec2f> landmark_collection;
			for (int i = 0; i < landmarks.size(); ++i)
			{
				landmark_collection.push_back(core::Landmark<cv::Vec2f>{ landmark_ids[i], cv::Vec2f(landmarks[i].x, landmarks[i].y) });
			}
			auto result = fitting::fit_shape_and_pose(morphable_model, blendshapes, landmark_collection, landmark_mapper, image_width, image_height, edge_topology, contour_landmarks, model_contour, num_iterations, num_shape_coefficients_opt, lambda, boost::none, pca_coeffs, blendshape_coeffs, fitted_image_points);
			return std::make_tuple(result.first, result.second, pca_coeffs, blendshape_coeffs);
		}, "Fit the pose (camera), shape model, and expression blendshapes to landmarks, in an iterative way. Returns a tuple (mesh, rendering_parameters, shape_coefficients, blendshape_coefficients).", py::arg("morphable_model"), py::arg("blendshapes"), py::arg("landmarks"), py::arg("landmark_ids"), py::arg("landmark_mapper"), py::arg("image_width"), py::arg("image_height"), py::arg("edge_topology"), py::arg("contour_landmarks"), py::arg("model_contour"), py::arg("num_iterations") = 5, py::arg("num_shape_coefficients_to_fit") = -1, py::arg("lambda") = 30.0f)
		;

    return eos_module.ptr();
};