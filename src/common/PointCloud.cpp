// Copyright (c) 2017-2019, The Johns Hopkins University /
// Applied Physics Laboratory (JHU/APL)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// Please reference the following when reporting any results using this software:
//
// M. Bosch, A. Leichtman, D. Chilcott, H. Goldberg, M. Brown, “Metric
// Evaluation Pipeline for 3D Modeling of Urban Scenes,” ISPRS Archives, 2017.
//
// S. Almes, S. Hagstrom, D. Chilcott, H. Goldberg, M. Brown, “Open Source
// Geospatial Tools to Enable Large Scale 3D Scene Modeling,” FOSS4G, 2017.
//
// For more information, please see: http://www.jhuapl.edu/pubgeo.html

#include "PointCloud.h"
#ifdef WIN32
#include <regex>
#endif

// Pipeline needs to read in point cloud file of any type, and read it in. ideally in meters
static std::string PDAL_PIPELINE_OPEN_ENGINE = R"({ "pipeline": [ ")";
static std::string PDAL_PIPELINE_OPEN_CABOOSE = R"("] } )";

std::string buildPipelineStr(std::string fileName) {
#ifdef WIN32
	fileName = std::regex_replace(fileName, std::regex("\\\\"), "/"); // Replace single backslash with double
#endif
    return PDAL_PIPELINE_OPEN_ENGINE + fileName + PDAL_PIPELINE_OPEN_CABOOSE;
}

namespace pubgeo {
    PointCloud::PointCloud() : zone(0), numPoints(0), xOff(0), yOff(0), zOff(0), executor(nullptr), pv(nullptr) {
        bounds = MinMaxXYZ{0, 0, 0, 0, 0, 0};
    }

    PointCloud::~PointCloud() {
        CleanupPdalPointers();
    }

    bool PointCloud::Read(const char *fileName) {
        try {
            CleanupPdalPointers();
            executor = new pdal::PipelineManager();
            std::stringstream ss(buildPipelineStr(fileName));
            executor->readPipeline(ss);
            executor->execute();
            const pdal::PointViewSet &pvs = executor->views();
            if (pvs.size() > 1) {
                std::cerr << "[PUBGEO::PointCloud::READ] File contains additional unread sets." << std::endl;
            }

            return Read(*pvs.begin());
        }
        catch (pdal::pdal_error &pe) {
            std::cerr << pe.what() << std::endl;
            return false;
        }
    }

    bool PointCloud::Read(pdal::PointViewPtr view) {
        pv = view;
        numPoints = pv->size();
        if (numPoints < 1) {
            std::cerr << "[PUBGEO::PointCloud::READ] No points found in file." << std::endl;
            return false;
        }

        pdal::BOX3D box;
        pv->calculateBounds(box);
        zone = pv->spatialReference().getUTMZone();

        // used later to return points
        xOff = (int) floor(box.minx);
        yOff = (int) floor(box.miny);
        zOff = (int) floor(box.minz);

        bounds = {box.minx, box.maxx, box.miny, box.maxy, box.minz, box.maxz};
        return true;
    }


    bool PointCloud::TransformPointCloud(std::string inputFileName, std::string outputFileName,
                                         float translateX = 0, float translateY = 0, float translateZ = 0) {
#ifdef WIN32
		inputFileName = std::regex_replace(inputFileName, std::regex("\\\\"), "/"); // Replace single backslash with double
		outputFileName = std::regex_replace(outputFileName, std::regex("\\\\"), "/"); // Replace single backslash with double
#endif
        std::ostringstream pipeline;
        pipeline << "{\n\t\"pipeline\":[\n\t\t\"" << inputFileName
                 << "\",\n\t\t{\n\t\t\t\"type\":\"filters.transformation\",\n"
                 << "\t\t\t\"matrix\":\""
                 << "1 0 0 " << translateX << " "
                 << "0 1 0 " << translateY << " "
                 << "0 0 1 " << translateZ << " "
                 << "0 0 0 1\"\n\t\t},\n\t\t{"
                 << "\n\t\t\t\"filename\":\"" << outputFileName << "\"\n\t\t}\n\t]\n}";
        try {
            const std::string pipe = pipeline.str();
            pdal::PipelineManager executor;
            executor.readPipeline(pipe);
            executor.execute();
        } catch (pdal::pdal_error &pe) {
            std::cerr << pe.what() << std::endl;
            return false;
        }
        return true;
    }

    PointCloud PointCloud::CropToClass(int keep_class) {
        pdal::PointViewPtr outView = pv->makeNew();
        for (pdal::PointId idx = 0; idx < pv->size(); ++idx)
            if (c(idx)==keep_class)
                outView->appendPoint(*pv,idx);
        PointCloud out;
        out.Read(outView);
        return out;
    }

    void PointCloud::CleanupPdalPointers() {
        if (pv != nullptr) {
            pv = nullptr;
        }
        if (executor != nullptr) {
            delete executor;
            executor = nullptr;
        }
    }

};
