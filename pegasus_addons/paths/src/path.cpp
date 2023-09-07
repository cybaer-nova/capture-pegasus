/****************************************************************************
 *
 *   Copyright (C) 2023 Marcelo Jacinto. All rights reserved.
 *   Author: Marcelo Jacinto <marcelo.jacinto@tecnico.ulisboa.pt>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name Pegasus nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/
#include "paths/path.hpp"
#include <algorithm>

namespace Pegasus::Paths {
/**
 * @brief Construct a new Path object
 */
Path::Path() {

}

/**
 * @brief Destroy the Path object
 */
Path::~Path() {

}

/**
 * @brief Add a path section shared pointer to the end of the path
 * @param section A shared pointer to a generic path section
 */
void Path::push_back(const Section::SharedPtr section) {

    // Add a new section to the path
    sections_.push_back(section);

    // If we already had samples in the samples vector, then add this new one as well
    // otherwise, wait for lazy initialization
    if(!samples_.empty()) {

        // Get the section samples
        auto section_samples = section->get_samples(sample_step_size_);

        // Add them to the samples_ vector by reference instead of copy
        samples_.insert(samples_.end(), std::make_move_iterator(section_samples.begin()), std::make_move_iterator(section_samples.end()));
    }
}

/**
 * @brief Get a reference to a section, given the parametric value
 * @param gamma The parametric value
 * @return Section::SharedPtr 
 */
Section::SharedPtr Path::get_section(double gamma) {

    // Bound the gamma between the minimum and maximum values
    double t = std::min(std::max(0.0, gamma), static_cast<double>(sections_.size()));

    // Get the index of the section in the sections vector
    std::optional<unsigned int> index = get_section_index(t);

    // Check if there is a section for the given gamma. If so return a shared pointer to it, otherwise return null
    return (index.has_value()) ? sections_[index.value()] : nullptr;
}

/**
 * @brief Get the index of a given section for a parametric value inside the 
 * sections vector
 * @param gamma The parametric value
 * @return unsigned int The index inside the section vector
 */
std::optional<unsigned int> Path::get_section_index(double gamma) {
    // Given that each section is normalized between 0 and 1, then if we floor a given gamma, let's say gamma = 0.5
    // then we know that it belongs to section of index=0.
    // If we got gamma = 5.9, then it belonged to the section of index=5 and we could peak its properties
    // if we query section of index=5 with gamma=0.9

    unsigned int index = std::floor(gamma);
    if(index >= sections_.size()) index = sections_.size() - 1;

    return (!empty()) ? std::make_optional<unsigned int>(index) : std::nullopt;
}

/**
 * @brief Get the string which describes the type of path section that is setored for a particular
 * parametric value of the full path
 * @param gamma The parametric value
 * @return std::string A description of the path section type
 */
std::optional<std::string> Path::get_section_type(double gamma) {

    // Get the index of the section in the sections vector
    std::optional<unsigned int> index = get_section_index(gamma);

    // If there is a section for the given gamma, then access it and get its type (can be a line, circle, arc, lemniscate, ...)
    return (index.has_value()) ? std::optional<std::string>(sections_[index.value()]->get_section_type()) : std::nullopt;
}

/**
 * @brief Clear the path (removes all segments if any were added)
 */
void Path::clear() {
    // Empty the sections vector
    sections_.clear();
    
    //Empty the samples vector
    samples_.clear();
}

/**
 * @brief Method that given a gamma will return a Path::Data
 * structure containing all the metrics of the path evaluated at that parametric value
 * @param gamma The path parameter
 * @return std::optional<Data>
 */
std::optional<Path::Data> Path::get_all_data(double gamma) {

    // Get the index of the section in the sections vector
    std::optional<unsigned int> index = get_section_index(gamma);

    if(index.has_value()) {

        // Make the gamma vary between 0 and 1 for a given path section
        double normalized_gamma = gamma - static_cast<double>(index.value());

        // Get the position, first and second derivatives
        data_.pd = sections_[index.value()]->pd(normalized_gamma);
        data_.d_pd = sections_[index.value()]->d_pd(normalized_gamma);
        data_.dd_pd = sections_[index.value()]->dd_pd(normalized_gamma);

        // Get other statistics
        data_.curvature = sections_[index.value()]->curvature(normalized_gamma);
        data_.torsion = sections_[index.value()]->torsion(normalized_gamma);
        data_.tangent_angle = sections_[index.value()]->tangent_angle(normalized_gamma);
        data_.derivative_norm = sections_[index.value()]->derivative_norm(normalized_gamma);

        // Get the speed assignments
        data_.vehicle_speed = sections_[index.value()]->vehicle_speed(normalized_gamma);
        data_.vd = sections_[index.value()]->vd(normalized_gamma);
        data_.d_vd = sections_[index.value()]->d_vd(normalized_gamma);
        
        // Get the bounds of the path
        data_.min_gamma = 0;
        data_.max_gamma = sections_.size();

        // Return an optional containing the reference to this structure
        return std::make_optional<Path::Data>(data_);

    } else {
        // Return an empty optional
        return std::nullopt;
    }
    
}

/**
 * @brief The section parametric equation 
 * @param gamma The path parameter
 * @return std::optional<Eigen::Vector3d> The equation of the path evaluated at the path parameter gamma
 */
std::optional<Eigen::Vector3d> Path::pd(double gamma) {

    // Get the index of the section in the sections vector
    std::optional<unsigned int> index = get_section_index(gamma);

    // If there is a section for a given gamma, then access it and return the position, otherwise return a null optional
    if(index.has_value()) {
       
        // Make the gamma vary between 0 and 1 for a given path section
        double normalized_gamma = gamma - static_cast<double>(index.value());

        return std::optional<Eigen::Vector3d>(sections_[index.value()]->pd(normalized_gamma));
    }
    
    return std::nullopt;
}

/**
 * @brief First derivative of the path section equation with respect to path parameter gamma
 * @param gamma The path parameter
 * @return std::optional<Eigen::Vector3d> The second derivative of the path equation evaluated at the path parameter gamma
 */
std::optional<Eigen::Vector3d> Path::d_pd(double gamma) {

    // Get the index of the section in the sections vector
    std::optional<unsigned int> index = get_section_index(gamma);

    // If there is a section for a given gamma, then access it and return the position, otherwise return a null optional
    if(index.has_value()) {
        // Make the gamma vary between 0 and 1 for a given path section
        double normalized_gamma = gamma - static_cast<double>(index.value());

        return std::optional<Eigen::Vector3d>(sections_[index.value()]->d_pd(normalized_gamma));
    }

    return std::nullopt;

}

/**
 * @brief Second derivative of the path section equation with respect to the path parameter gamma
 * @param gamma  The path parameter
 * @return std::optional<double> The second derivative of the path equation evaluated at the path parameter gamma
 */
std::optional<Eigen::Vector3d> Path::dd_pd(double gamma) {

    // Get the index of the section in the sections vector
    std::optional<unsigned int> index = get_section_index(gamma);

    // If there is a section for a given gamma, then access it and return the position, otherwise return a null optional
    if(index.has_value()) {
        // Make the gamma vary between 0 and 1 for a given path section
        double normalized_gamma = gamma - static_cast<double>(index.value());

        return std::optional<Eigen::Vector3d>(sections_[index.value()]->dd_pd(normalized_gamma));
    }

    return std::nullopt;
}

/**
 * @brief Default method for computing the curvature. The default implementation
 * implements the general formula to compute the curvature based on the derivative
 * equations of the path
 * @param gamma The path parameter
 * @return std::optional<double> The path curvature 
 */
std::optional<double> Path::curvature(double gamma) {

    // Get the index of the section in the sections vector
    std::optional<unsigned int> index = get_section_index(gamma);

    // If there is a section for a given gamma, then access it and return the position, otherwise return a null optional
    if(index.has_value()) {

        // Make the gamma vary between 0 and 1 for a given path section
        double normalized_gamma = gamma - static_cast<double>(index.value());
        
        return std::optional<double>(sections_[index.value()]->curvature(normalized_gamma));
    }
    return std::nullopt;
}

/**
 * @brief Default method for computing the torsion. The default implementation
 * implements the general formula to compute the curvature based on the derivative
 * equations of the path
 * @param gamma The path parameter
 * @return std::optional<double> The path torsion
 */
std::optional<double> Path::torsion(double gamma) {

    // Get the index of the section in the sections vector
    std::optional<unsigned int> index = get_section_index(gamma);

    // If there is a section for a given gamma, then access it and return the position, otherwise return a null optional
    if(index.has_value()) {
        // Make the gamma vary between 0 and 1 for a given path section
        double normalized_gamma = gamma - static_cast<double>(index.value());

        return std::optional<double>(sections_[index.value()]->torsion(normalized_gamma));
    }
    
    return std::nullopt;
}

/**
 * @brief Default method for computing the tangent angle to the path 
 * @param gamma The path parameter
 * @return std::optional<double> The angle of the tangent to the path expressed in radians
 */
std::optional<double> Path::tangent_angle(double gamma) {
    
    // Get the index of the section in the sections vector
    std::optional<unsigned int> index = get_section_index(gamma);
    
    // If there is a section for a given gamma, then access it and return the position, otherwise return a null optional
    if(index.has_value()) {
        // Make the gamma vary between 0 and 1 for a given path section
        double normalized_gamma = gamma - static_cast<double>(index.value());

        return std::optional<double>(sections_[index.value()]->tangent_angle(normalized_gamma));
    }

    return std::nullopt;
}

/**
 * @brief Default method for computing the norm of the derivative 
 * @param gamma  The path parameter
 * @return std::optional<double> The norm of the derivative of the path position pd
 */
std::optional<double> Path::derivative_norm(double gamma) {

    // Get the index of the section in the sections vector
    std::optional<unsigned int> index = get_section_index(gamma);

    // If there is a section for a given gamma, then access it and return the position, otherwise return a null optional
    if(index.has_value()) {
        // Make the gamma vary between 0 and 1 for a given path section
        double normalized_gamma = gamma - static_cast<double>(index.value());

        return std::optional<double>(sections_[index.value()]->derivative_norm(normalized_gamma));
    }

    return std::nullopt;
}

/**
 * @brief Default method for getting the desired vehicle speed for a particular 
 * location in the path section (in m/s)
 * @param gamma The path parameter
 * @return std::optional<double> The desired vehicle speed (in m/s)
 */
std::optional<double> Path::vehicle_speed(double gamma) {
    
    // Get the index of the section in the sections vector
    std::optional<unsigned int> index = get_section_index(gamma);

    // If there is a section for a given gamma, then access it and return the position, otherwise return a null optional
    if(index.has_value()) {
        // Make the gamma vary between 0 and 1 for a given path section
        double normalized_gamma = gamma - static_cast<double>(index.value());

        return std::optional<double>(sections_[index.value()]->vehicle_speed(normalized_gamma));
    }

    return std::nullopt;
}

/**
 * @brief Default method for getting the desired speed for the evolution
 * of the parametric value
 * @param The path parameter
 * @return std::optional<double> The desired speed progression of the parametric variable
 */
std::optional<double> Path::vd(double gamma) {
    
    // Get the index of the section in the sections vector
    std::optional<unsigned int> index = get_section_index(gamma);

    // If there is a section for a given gamma, then access it and return the position, otherwise return a null optional
    if(index.has_value()) {
        // Make the gamma vary between 0 and 1 for a given path section
        double normalized_gamma = gamma - static_cast<double>(index.value());

        return std::optional<double>(sections_[index.value()]->vd(normalized_gamma));
    }

    return std::nullopt;
}

/**
 * @brief Default method for getting the desired acceleration for the evolution
 * of the parametric value
 * @param gamma The path parameter
 * @return std::optional<double> The speed progression of the parametric variable
 */
std::optional<double> Path::d_vd(double gamma) {

    // Get the index of the section in the sections vector
    std::optional<unsigned int> index = get_section_index(gamma);

    // If there is a section for a given gamma, then access it and return the position, otherwise return a null optional
    if(index.has_value()) {

        // Make the gamma vary between 0 and 1 for a given path section
        double normalized_gamma = gamma - static_cast<double>(index.value());

        return std::optional<double>(sections_[index.value()]->d_vd(normalized_gamma));
    }

    return std::nullopt;
}

/**
 * @brief Get the end position of the path corresponding to the maximum gamma value
 * @return std::optional<Eigen::Vector3d> The last pd of the path or std::nullopt
 */
std::optional<Eigen::Vector3d> Path::get_last_pd() {

    // Check if the path is emtpy
    if(empty()) return std::nullopt;

    // Get the last section of the path and return an optional with pd
    auto last_section = sections_.back();
    return std::make_optional<Eigen::Vector3d>(last_section->pd(1.0));
}

/**
 * @brief Method that outputs a bound parametric value between minimum and the maximum
 * parametric value that a path can achieve based on the maximum number of sections it contains
 * at any given time.
 * @param gamma The path parameter
 * @return double The bounded path parameter
 */
double Path::bound_gamma(double gamma) {
    // With substract a very small number because we can only access the vector
    // at the elements [0, number_sections[
    return std::min(std::max(0.0, gamma), (double) sections_.size() - 0.0000000001);
}

/**
 * @brief Method to get a set of sampled points from the path
 * @param step_size The step size to increment the parametric value gamma for sampling purposes. Smaller
 * gammas yield a finner result but also more points
 * @return std::vector<Eigen::Vector3d> A vector of 3d points
 */
 std::optional<std::vector<Eigen::Vector3d>> Path::get_samples(double step_size) {
    
    // If the path is empty, just return a nullopt
    if(sections_.empty()) return std::nullopt;

    // Check if the used step size used for previous sampling was the same as the request one
    bool step_size_is_equal = std::abs(sample_step_size_ - step_size) < 0.0000000001;

    // If the previous step size used for sampling is different then the current one, clear all the samples we previously had
    if(!step_size_is_equal) samples_.clear();

    // Check if the path is not empty, but the samples have not been initialized
    bool samples_initialized = samples_.empty(); 

    // Obtain new samples if either the samples vector was not initialized or 
    // the previous requested step size is different
    // NOTE: we are doing lazy initialization!
    if(samples_initialized == false || step_size_is_equal == false) {

        // Update the new sample step size used for sampling
        sample_step_size_ = step_size;
        
        for (auto & section: sections_) {
            // Get the section samples
            auto section_samples = section->get_samples(sample_step_size_);

            // Add them to the samples_ vector by reference instead of copy
            samples_.insert(samples_.end(), std::make_move_iterator(section_samples.begin()), std::make_move_iterator(section_samples.end()));
        }
    }

    // return the optional containing the vector of points that represent the path
    return std::make_optional<std::vector<Eigen::Vector3d>>(samples_);
}

/**
 * @brief Get the min value accepted for the parametric variable
 * @return double A double with the minimum value for gamma (tipically its 0.0)
 */
double Path::get_min_gamma() {
    return 0.0;
}

/**
 * @brief Get the max value accepted for the parametric variable
 * @return double A double with the maximum value for gamma (tipicaly its the number of segments the path containts)
 */
double Path::get_max_gamma() {
    return (sections_.empty()) ? 0.0 : (double) sections_.size();
}

}