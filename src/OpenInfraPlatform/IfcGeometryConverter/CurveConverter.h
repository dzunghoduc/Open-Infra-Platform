/* -*-c++-*- IfcPlusPlus - www.ifcplusplus.com  - Copyright (C) 2011 Fabian Gerold
 * This library is open source and may be redistributed and/or modified under
 * the terms of the OpenSceneGraph Public License (OSGPL) version 0.0 or
 * (at your option) any later version.  The full license is in LICENSE file
 * included with this distribution, and on the openscenegraph.org website.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * OpenSceneGraph Public License for more details.
 *
 * - modfied by Michael Kern (September 2014)
*/

// visual studio
#pragma once
// unix
#ifndef CURVECONVERTER_H
#define CURVECONVERTER_H

#define _USE_MATH_DEFINES 
#include <math.h>

#include "CarveHeaders.h"

#include "GeomUtils.h"
#include "UnhandledRepresentationException.h"
#include "PlacementConverter.h"
#include "ProfileConverter.h"
#include "GeometrySettings.h"
#include "SplineConverter.h"


#include "OpenInfraPlatform/ExpressBinding/vectorUtilites.h"

/**********************************************************************************************/

namespace OpenInfraPlatform {
	namespace IfcGeometryConverter {
		template<
			class IfcEntityTypesT,
			class IfcUnitConverterT
		>
			class CurveConverterT {
			public:
				CurveConverterT(std::shared_ptr<GeometrySettings> geomSettings,
					std::shared_ptr<IfcUnitConverterT> unitConverter)
					:
					geomSettings(geomSettings),
					unitConverter(unitConverter)
				{
				}

				virtual ~CurveConverterT()
				{

				}

				void convertIfcCurve(const std::shared_ptr<typename IfcEntityTypesT::IfcCurve>& ifcCurve,
					std::vector<carve::geom::vector<3>>& loops,
					std::vector<carve::geom::vector<3>>& segmentStartPoints) const
				{
					std::vector<std::shared_ptr<typename IfcEntityTypesT::IfcTrimmingSelect> > trim1Vec;
					std::vector<std::shared_ptr<typename IfcEntityTypesT::IfcTrimmingSelect> > trim2Vec;
					convertIfcCurve(ifcCurve, loops, segmentStartPoints, trim1Vec, trim2Vec, true);
				}

				void convertIfcCurve(const std::shared_ptr<typename IfcEntityTypesT::IfcCurve>& ifcCurve,
					std::vector<carve::geom::vector<3>>& targetVec,
					std::vector<carve::geom::vector<3>>& segmentStartPoints,
					std::vector<std::shared_ptr<typename IfcEntityTypesT::IfcTrimmingSelect> >& trim1Vec,
					std::vector<std::shared_ptr<typename IfcEntityTypesT::IfcTrimmingSelect> >& trim2Vec,
					bool senseAgreement) const
				{
					double length_factor = unitConverter->getLengthInMeterFactor();
					double plane_angle_factor = unitConverter->getAngleInRadianFactor();

					/*	CurveConverter.h
					For IFC4x1:
					
					IfcCurve

					IfcBoundedCurve				http://www.buildingsmart-tech.org/ifc/IFC4x1/RC3/html/schema/ifcgeometryresource/lexical/ifcboundedcurve.htm
						IfcAlignmentCurve
						IfcBsplineCurve
							IfcBsplineCurveWithKnots*
								IfcRationalBsplineCurveWithKnots*
						IfcCompositeCurve
							IfcCompositeCurveOnSurface*
						IfcIndexedPolycurve*
						IfcPolyline
						IfcTrimmedCurve

					IfcConic					http://www.buildingsmart-tech.org/ifc/IFC4x1/RC3/html/schema/ifcgeometryresource/lexical/ifcconic.htm
						IfcCircle
						IfcEllipse

					IfcLine						http://www.buildingsmart-tech.org/ifc/IFC4x1/RC3/html/schema/ifcgeometryresource/lexical/ifcline.htm		
					
					IfcOffsetCurve				http://www.buildingsmart-tech.org/ifc/IFC4x1/RC3/html/schema/ifcgeometryresource/lexical/ifcoffsetcurve.htm
						IfcOffsetCurve2D*
						IfcOffsetCurve3D*
						IfcOffsetCurveByDistances*
						
					IfcPcurve*					http://www.buildingsmart-tech.org/ifc/IFC4x1/RC3/html/schema/ifcgeometryresource/lexical/ifcpcurve.htm
					
					IfcSurfaceCurve				http://www.buildingsmart-tech.org/ifc/IFC4x1/RC3/html/schema/ifcgeometryresource/lexical/ifcsurfacecurve.htm
						IfcIntersectionCurve*
						IfcSeamCurve*

					Functions

					*: not implemented in CurveConverter.h

					*/

				// ************************************************************************************************************************************	//
				//	IfcBoundedCurve SUPTYPE of IfcCurve																									//
				//	ABSTRACT SUPERTYPE of IfcAlignmentCurve, IfcBsplineCurve, IfcCompositeCurve, IfcIndexedPolycurve, IfcPolyline, IfcIfcTrimmedCurve	//
				// ************************************************************************************************************************************	//
					std::shared_ptr<typename IfcEntityTypesT::IfcBoundedCurve> bounded_curve =
						dynamic_pointer_cast<typename IfcEntityTypesT::IfcBoundedCurve>(ifcCurve);
					if(bounded_curve) 
					{
					// (1/6) IfcAlignmentCurve SUBTYPE OF IfcBoundedCurve
					std::shared_ptr<typename IfcEntityTypesT::IfcAlignmentCurve> alignment_curve =
						dynamic_pointer_cast<typename IfcEntityTypesT::IfcAlignmentCurve>(bounded_curve);
					if(alignment_curve) {

						//	********************************************************************************************
						//	PART 1/4. Class Definitions.
						//	********************************************************************************************

						// Station class.
						class Station {
						public:
							double distAlong_;	// horizontal distance along.
							double x_, y_, z_;	// 3D-coordinates of station.

							Station(double distAlong, double x, double y, double z)
								: distAlong_(distAlong), x_(x), y_(y), z_(z)
							{
							}
							~Station() {}
						};

						// Segment classes.
						class Segment {
						public:
							std::vector<Station> segmentStations_;											// vector of all stations within segment (horizontalSegmentStations or verticalSegmentStations)
							enum segmentType { isLine, isCircArc, isTrans, isParArc } segType_;				// segment type.
							int fragmentsCount_ = 0;														// number of fragments within segment. 
							double fragmentsLength_;														// length of one fragment within segment.

							Segment(std::vector<Station> segmentStations, segmentType segType, int fragmentsCount, double fragmentsLength
								: segmentStations_(segmentStations), segType_(segType), fragmentsCount_(fragmentsCount), fragmentsLength_(fragmentsLength))
							{
							}
							~Segment() {}

							void addStation(double newStationDistAlong)
							{
								segmentStations_.push_back(Station(newStationDistAlong, 0, 0, 0));
							}

							void addStation(double newStationDistAlong, double z)
							{
								segmentStations_.push_back(Station(newStationDistAlong, 0, 0, z));
							}

							void addStation(double newStationDistAlong, double x, double y)
							{
								segmentStations_.push_back(Station(newStationDistAlong, x, y, 0));
							}

							void addStation(double newStationDistAlong, double x, double y, double z)
							{
								segmentStations_.push_back(Station(newStationDistAlong, x, y, z));
							}

							void insertStationAtSecondToLast(double newStationDistAlong, double x, double y, double z)
							{
								segmentStations_.insert(segmentStations_.end() - 1, Station(newStationDistAlong, x, y, z));
							}
						};
						class HorizontalSegment : public Segment {
						public:
							double startX_;
							double startY_;
							double startDirection_;
							double segmentLength_;
							std::weak_ptr<typename IfcEntityTypesT::IfcAlignment2DHorizontalSegment> itHorizontal_;	// pointer to specific horizontal segment.

							HorizontalSegment(double startX, double startY, double startDirection, double segmentLength, std::weak_ptr<typename IfcEntityTypesT::IfcAlignment2DSegment> > itHorizontal// members of HorizontalSegment
								std::vector<Station> segmentStations, segmentType segType, int fragmentsCount, double fragmentsLength) // members of Segment
								: Segment(segmentStations, segType, fragmentsCount, fragmentsLength),
								startX_(startX), startY_(startY), startDirection_(startDirection), segmentLength_(segmentLength), itHorizontal_(itHorizontal)
							{
							}
							~HorizontalSegment() {};
						};
						class VerticalSegment : public Segment {
						public:
							double startZ_;
							double startDistAlong_;
							double horizontalLength_;
							double startGradient_;
							std::weak_ptr<typename IfcEntityTypesT::IfcAlignment2DVerticalSegment> itVertical_;	// pointer to specific vertical segment.

							VerticalSegment(double startZ, double startDistAlong, double horizontalLength, double startGradient, std::weak_ptr<typename IfcEntityTypesT::IfcAlignment2DSegment> > itVertical, // members of VerticalSegment
								std::vector<Station> segmentStations, segmentType segType, int fragmentsCount, double fragmentsLength) // members of Segment
								: Segment(segmentStations, segType, fragmentsCount, fragmentsLength, itPointer),
								startZ_(startZ), startDistAlong_(startDistAlong), horizontalLength_(horizontalLength), startGradient_(startGradient), itVertical_(itVertical)
							{
							}
							~VerticalSegment() {};
						};

						// **********************************************************************************************************
						// PART 2/4. IfcAlignment2DHorizontal.
						//
						//		Step 1: Get segment information and store in horizontal station vector.
						//		Step 2: Get segment type information and additional members and store in horizontal segments vector.
						// **********************************************************************************************************

							// Step 1: Get segment information and store in horizontal station vector.

								// Declaration of horizontal segments vector.
						std::vector<HorizontalSegment> HorSegmentsVec;

						// Get information from alignment.
						std::shared_ptr<typename IfcEntityTypesT::IfcAlignment2DHorizontal> horizontal = alignment_curve->Alignment2DHorizontal;

						if(!horizontal) {
							BLUE_LOG(error) << "No IfcAlignment2DHorizontal in IfcAlignmentCurve."
								return;
						}

						// StartDistAlong type IfcLengthMeasure [0:1]
						double horStartDistAlong = 0.;
						double hStartDistAlong = horizontal->StartDistAlong;

						if(hStartDistAlong) {
							horStartDistAlong = hStartDistAlong * length_factor;
						}

						// Segments type IfcAlignment2DHorizontalSegment L[1:?]
						if(!horizontal->Segments) {
							BLUE_LOG(error) << "Not enough segments in IfcAlignment2DHorizontal. (Segment ID: " << Segments->getId() << ").";
							return;
						}

						std::vector<std::shared_ptr<typename IfcEntityTypesT::IfcAlignment2DHorizontalSegment> > horSegments =
							horizontal->Segments;

						if(horSegments.size() < 1) {
							BLUE_LOG(error) << "Not enough segments in IfcAlignment2DHorizontal. (Segment ID: " << horSegments->getId() << ").";
							return;
						}

						// Iterate over horizontal segments
						for(auto it_segment : horSegments) {
							//IfcAlignment2DHorizontalSegment (TangentialContinuity type IfcBoolean [0:1], StartTag type IfcLabel [0:1], EndTag type IfcLabel [0:1], CurveGeometry type IfcCurveSegment2D [1:1])
							std::shared_ptr<typename IfcEntityTypesT::IfcCurveSegment2D> horCurveGeometry =
								it_segment->CurveGeometry;

							// Get and interpret information from IfcCurveSegment2D.
							if(!horCurveGeometry) {
								BLUE_LOG(error) << "No curve geometry in IfcAlignment2DHorizontalSegment (Segment ID: " << it_segment->getId() << ").";
								return;
							}

							// StartPoint type IfcCartesianPoint [1:1]
							std::shared_ptr<typename IfcEntityTypesT::IfcCartesianPoint> curveSegStartPoint =
								horCurveGeometry->StartPoint;
							if(!curveSegStartPoint) {
								BLUE_LOG(error) << "No curve segment start point in IfcCurveSegment2D (Segment ID: " << it_segment->getId() << ").";
								return;
							}

							double xStart = 0., yStart = 0.;
							xStart = curveSegStartPoint->Coordinates[0] * length_factor;
							yStart = curveSegStartPoint->Coordinates[1] * length_factor;

							// StartDirection type IfcPlaneAngleMeasure [1:1]
							if(!horCurveGeometry->StartDirection) {
								BLUE_LOG(error) << "No curve segment start direction in IfcCurveSegment2D (Segment ID: " << it_segment->getId() << ").";
								return;
							}
							double dStartDirection = horCurveGeometry->StartDirection * plane_angle_factor;

							// SegmentLength type IfcPositiveLengthMeasure [1:1]
							if(!horCurveGeometry->SegmentLength) {
								BLUE_LOG(error) << "No curve segment length in IfcCurveSegment2D (Segment ID: " << it_segment->getId() << ").";
								return;
							}
							double dSegLength = horCurveGeometry->SegmentLength * length_factor;

							// Store information in station object.
							Station horizontalStation(horStartDistAlong, xStart, yStart, 0);

							// Step 2: Get segment type information and additional members and store in horizontal segments vector.

									// Segment types: IfcLineSegment2D, IfcCircularArcSegment2D and IfcTransitionCurveSegment2D
									// http://www.buildingsmart-tech.org/ifc/IFC4x1/final/html/schema/ifcgeometryresource/lexical/ifccurvesegment2d.htm
							std::shared_ptr<typename IfcEntityTypesT::IfcLineSegment2D> line_segment_2D =
								dynamic_pointer_cast<typename IfcEntityTypesT::IfcLineSegment2D>(horCurveGeometry);
							std::shared_ptr<typename IfcEntityTypesT::IfcCircularArcSegment2D> circular_arc_segment_2D =
								dynamic_pointer_cast<typename IfcEntityTypesT::IfcCircularArcSegment2D>(horCurveGeometry);
							std::shared_ptr<typename IfcEntityTypesT::IfcTransitionCurveSegment2D> trans_curve_segment_2D =
								dynamic_pointer_cast<typename IfcEntityTypesT::IfcTransitionCurveSegment2D>(horCurveGeometry);

							// Set number of fragments (number of points to be added between stations) according to segment type.
							if(line_segment_2D) {
								int nHorFragments = 0;
								HorizontalSegment horizontalSegment(xStart, yStart, dStartDirection, dSegLength, it_segment, { horizontalStation }, isLine, nHorFragments, dSegLength);
								HorSegmentsVec.push_back(horizontalSegment);
							}
							if(circular_arc_segment_2D) {
								int nHorFragments = geomSettings->min_num_vertices_per_arc;
								double nHorFragmentsLength = dSegLength / nHorFragments;

								HorizontalSegment horizontalSegment(xStart, yStart, dStartDirection, dSegLength, it_segment, { horizontalStation }, isCircArc, nHorFragments, nHorFragmentsLength);
								HorSegmentsVec.push_back(horizontalSegment);
							}
							if(trans_curve_segment_2D) {
								nHorFragments = geomSettings->min_num_vertices_per_arc;
								double nHorFragmentsLength = dSegLength / nHorFragments;

								HorizontalSegment horizontalSegment(xStart, yStart, dStartDirection, dSegLength, it_segment, { horizontalStation }, isTrans, nHorFragments, dSegLength);
								HorSegmentsVec.push_back(horizontalSegment);
							}

						} // end of HorSegments iteration

				// *******************************************************************************************************
				// PART 3/4. IfcAlignment2DVertical.
				//
				//		Step 1: Get segment information and store in vertical station vector.
				//		Step 2: Get segment type information and additional members and store in vertical segments vector.
				// *******************************************************************************************************

					// Step 1: Get segment information and store in vertical station vector.

						// Declaration of necessary objects and vectors.
						std::vector<VerticalSegment> VerSegmentsVec;

						// Get information.
						std::shared_ptr<typename IfcEntityTypesT::IfcAlignment2DVertical> vertical = alignment_curve->Alignment2DVertical;
						if(!vertical) {
							BLUE_LOG(warning) << "No IfcAlignment2DVertical in IfcAlignmentCurve (Segment ID: " << vertical->getId() << ").";
							// TO DO: Handle as horizontal alignment only.
						}
						else(vertical)
						{
							// Segments type IfcAlignment2DVerticalSegment segmentLength[1:?]
							if(!vertical->Segments) {
								BLUE_LOG(error) << "No segments in IfcAlignment2DVertical. (Segment ID: " << Segments->getId() << ").";
							}
							std::vector<std::shared_ptr<typename IfcEntityTypesT::IfcAlignment2DVerticalSegment> > verSegments =
								vertical->Segments;

							if(verSegments < 1) {
								BLUE_LOG(error) << "Not enough segments in IfcAlignment2DVertical. (Segment ID: " << verSegments->getId() << ").";
								return;
							}

							// Declaration of vertical stations and accuracy vectors.
							int nVerFragments = 0;

							// Iterate over vertical segments, get and interpret information. 
							for(auto it_segment : verSegments) {
								// StartDistAlong type IfcLengthMeasure [1:1]
								if(!it_segment->StartDistAlong) {
									BLUE_LOG(error) << "No start distance along in IfcAlignment2DVerticalSegment (Segment ID: " << it_segment->getId() << ").";
									return;
								}
								double verDistAlong = it_segment->StartDistAlong * length_factor;

								// HorizontalLength type IfcPositiveLengthMeasure [1:1]
								if(!it_segment->HorizontalLength) {
									BLUE_LOG(error) << "No horizontal length in IfcAlignment2DVerticalSegment (Segment ID: " << it_segment->getId() << ").";
									return;
								}
								double verHorizontalLength = it_segment->HorizontalLength * length_factor;

								// StartHeight type IfcLengthMeasure [1:1]
								if(!it_segment->StartHeight) {
									BLUE_LOG(error) << "No start height in IfcAlignment2DVerticalSegment (Segment ID: " << it_segment->getId() << ").";
									return;
								}
								double verStartHeight = it_segment->StartHeight * length_factor;

								// StartGradient type IfcRatioMeasure [1:1]
								if(!it_segment->StartGradient) {
									BLUE_LOG(error) << "No start gradient in IfcAlignment2DVerticalSegment (Segment ID: " << it_segment->getId() << ").";
									return;
								}
								double dVerStartGradient = it_segment->StartGradient;

								// Store information in station object.
								Station verticalStation(dVerDistAlong, 0, 0, zStart);

								// Step 2: Get segment type information and additional members and store in vertical segments vector.

											// Segment types: IfcAlignment2DVerSegCircularArc, IfcAlignment2DVerSegLine, IfcAlignment2DVerSegParabolicArc.
								std::shared_ptr<typename IfcEntityTypesT::IfcAlignment2DVerSegCircularArc> v_seg_circ_arc_2D =
									dynamic_pointer_cast<typename IfcEntityTypesT::IfcAlignment2DVerSegCircularArc>(it_segment);
								std::shared_ptr<typename IfcEntityTypesT::IfcAlignment2DVerSegLine> ver_seg_line_2D =
									dynamic_pointer_cast<typename IfcEntityTypesT::IfcAlignment2DVerSegLine>(it_segment);
								std::shared_ptr<typename IfcEntityTypesT::IfcAlignment2DVerSegParabolicArc> v_seg_par_arc_2D =
									dynamic_pointer_cast<typename IfcEntityTypesT::IfcAlignment2DVerSegParabolicArc>(it_segment);

								// Set number of fragments (number of stations to be added within segment) according to segment type.
								if(v_seg_circ_arc_2D) {
									nVerFragments = geomSettings->min_num_vertices_per_arc;
									double nVerFragmentsLength = dVerHorizontalLength / nVerFragments;
									VerticalSegment verticalSegment(zStart, dVerDistAlong, dVerHorizontalLength, dVerStartGradient, it_segment,
									{ verticalStation }, isLine, nVerFragments, nVerFragmentsLength);
									VerSegmentsVec.push_back(verticalSegment);
								}
								if(ver_seg_line_2D) {
									nVerFragments = 0;
									VerticalSegment verticalSegment(zStart, dVerDistAlong, dVerHorizontalLength, dVerStartGradient, it_segment,
									{ verticalStation }, isLine, nVerFragments, dVerHorizontalLength);
									VerSegmentsVec.push_back(verticalSegment);
								}
								if(v_seg_par_arc_2D) {
									nVerFragments = geomSettings->min_num_vertices_per_arc;
									double nVerFragmentsLength = dVerHorizontalLength / nVerFragments;
									VerticalSegment verticalSegment(zStart, dVerDistAlong, dVerHorizontalLength, dVerStartGradient, it_segment,
									{ verticalStation }, isLine, nVerFragments, nVerFragmentsLength);
									VerSegmentsVec.push_back(verticalSegment);
								}

							} // end of VerSegments iteration

				//	********************************************************************************************
				//	PART 4/4. Interpretation.
				//
				//		Step 1: Combine horizontal and vertical stations.
				//		Step 2: Calculate coordinates.
				//		Step 3: Create curve points vector with coordinates (x,y,z) for each station.
				//	********************************************************************************************

					// Step 1: Combine horizontal and vertical stations.

							int it_h = 0; // iterator for horizontal segments vector: HorSegmentsVec
							int it_v = 0; // iterator for vertical segments vector: VerSegmentsVec							
							double segmentStart = horStartDistAlong; // Find out where to start adding stations. (Assuming horizontal and vertical alignment start at the same distance along!)			
							int hVecSize = HorSegmentsVec.size();
							int vVecSize = VerSegmentsVec.size();

							// Iterate over horizontal and vertical segments, add stations according to fragments, sort and remove duplicate stations.
							while(it_h <= hVecSize) {
								double dHorDistAlong = HorSegmentsVec[it_h].distAlong_; // Horizontal segment's start distance along
								double dHorFragmentsLength = HorSegmentsVec[it_h].fragmentsLength_; // Horizontal segment's length of fragments

								while(it_v <= vVecSize) {
									double dVerDistAlong = VerSegmentsVec[it_v].distAlong_; // Vertical segment's start distance along
									double dVerFragmentsLength = VerSegmentsVec[it_v].fragmentsLength_; // Vertical segment's length of fragments

									// If segment start stations are not close, copy horizontal to vertical and vice-versa - but only where segments overlap.
									if(abs(dHorDistAlong - dVerDistAlong) <= 0.00000001) {
										// Check which station is missing.
										if(dHorDistAlong < dVerDistAlong)
											VerSegmentsVec[it_v].insertStationAtSecondToLast(dHorDistAlong);
										else
											HorSegmentsVec[it_h].insertStationAtSecondToLast(dVerDistAlong);
									}
									// If they are close, replace station that has greater distAlong and adapt segment length (TODO: also adapt fragments length?)
									else {
										if(dHorDistAlong < dVerDistAlong) {
											// Use deleteAllStations function because segment's stations vector only has one entry (its start station)
											VerSegmentsVec[it_v].deleteAllStations();
											VerSegmentsVec[it_v].addStation(dHorDistAlong);
											// Current and previous vertical segment's lengths have changed
											VerSegmentsVec[it_v].segmentLength_ -= abs(dHorDistAlong - dVerDistAlong);
											VerSegmentsVec[it_v - 1].segmentLength_ += abs(dHorDistAlong - dVerDistAlong);
										}
										else {
											HorSegmentsVec[it_h].deleteAllStations();
											HorSegmentsVec[it_h].addStation(dVerDistAlong);
											// Current and previous horizontal segment's lengths have changed
											HorSegmentsVec[it_h].segmentLength_ -= abs(dHorDistAlong - dVerDistAlong);
											HorSegmentsVec[it_h - 1].segmentLength_ += abs(dHorDistAlong - dVerDistAlong);
										}
									}

									// Compare length of fragments, choose smaller value (-> more accurate representation).
									double dFragmentsLength = (dVerFragmentsLength > dHorFragmentsLength) ? dHorFragmentsLength : dVerFragmentsLength;

									// Find end of shorter segment.
									bool shorterSegmentIsVertical = horizontalSegmentEnd >= verticalSegmentEnd ? true : false;
									double horizontalSegmentEnd = dHorDistAlong + HorSegmentsVec[it_h].segmentLength_;
									double verticalSegmentEnd = dVerDistAlong + VerSegmentsVec[it_v].horizontalLength_;
									double shorterSegmentEnd = shorterSegmentIsVertical ? verticalSegmentEnd : horizontalSegmentEnd;

									// Create equidistant stations between startDistanceAlong and shorterSegmentEnd.
									double distance = segmentStart;
									while(distance < shorterSegmentEnd) {
										distance += dFragmentsLength;
										HorSegmentsVec[it_h].addStation(distance);
										VerSegmentsVec[it_v].addStation(distance);
									}

									// Add last station separately to guarantee existence.
									horizontalAlignment.horizontalSegments_[it_h].addStation(shorterSegmentEnd);
									verticalAlignment.verticalSegments_[it_v].addStation(shorterSegmentEnd);
									// In next iteration step, the end of the shorter segment here is where we continue to add stations in the longer segment.
									segmentStart = shorterSegmentEnd;
									// Proceed to next segment - depending on which is shorter.
									shorterSegmentIsVertical ? it_v++ : it_h++;
								} // end while it_v
							} // end while it_h

					// Step 2: Calculate coordinates (According to segment type and segmentStations_).

							// HORIZONTAL.			
							// Iterate over horizontal segments and calculate missing x and y coordinates. 
							for(auto it_hor_segments : HorSegmentsVec) {
								// Get "global" segment information necessary for calculation via pointer to segment. 
								std::shared_ptr<typename IfcEntityTypesT::IfcCurveSegment2D> horCurveGeometry =
									it_hor_segments.itHorizontal_->CurveGeometry;

								double startStationX = it_hor_segments.itHorizontal_->segmentStations_[0]->x_;
								double startStationY = it_hor_segments.itHorizontal_->segmentStations_[0]->y_;
								double segmentLength = it_hor_segments.itHorizontal_->segmentLength_;
								enum SegType = it_hor_segments.itHorizontal_->segmentType_;

								// HORIZONTAL LINE SEGMENT.
								if(segType == isLine) {
									// Iterate over stations in line segment.
									for(auto it_hor_stations = it_hor_segments.itHorizontal_->segmentStations_.begin()) {
										// (1) Get "station-specific" segment information.
										double startDirection = it_hor_stations.startDirection_;

										// (1) Get linesegment information.
										// No additional information provided.

										// (3) Calculate x and y coordinates for each horizontal station within line segment. 
										if(it_hor_stations.x_ != 0) { // Skips calculation if x,y coordinate is already there.
											it_hor_stations.x_ = startStation.x_ + segmentLength * cos(it_hor_segments.startDirection_);
											it_hor_stations.y_ = startStation.y_ + segmentLength * sin(it_hor_segments.startDirection_);
										} // end if
									} // end stations iteration
								} // end if isLine

								// HORIZONTAL CIRCULAR ARC SEGMENT.
								if(segType == isCircArc) {
									// Iterate over stations in circular arc segment.
									for(auto it_hor_stations = it_hor_segments.itHorizontal_->segmentStations_.begin()) {

										// (1) Get "station-specific" segment information.
										double startDirection = it_hor_stations.startDirection_;

										// (2) Get circular arc segment information.
										std::shared_ptr<typename IfcEntityTypesT::IfcCircularArcSegment2D> circular_arc_segment_2D =
											dynamic_pointer_cast<typename IfcEntityTypesT::IfcCircularArcSegment2D>(horCurveGeometry);

										// Radius type IfcPositiveLengthMeasure [1:1]
										if(!circular_arc_segment_2D->Radius) {
											BLUE_LOG(error) << "No radius in IfcCircularArcSegment2D (Segment ID: " << it_segment->getId() << ").";
											return;
										}
										double radius = circular_arc_segment_2D->Radius * length_factor;

										// IsCCW type IfcBoolean [1:1]
										if(!circular_arc_segment_2D->IsCCW) {
											BLUE_LOG(error) << "No direction information for IfcCircularArcSegment2D (counterclockwise/clockwise) (Segment ID: " << it_segment->getId() << ").";
											return;
										}
										bool is_CCW = circular_arc_segment_2D->IsCCW;

										// (3) Calculate x and y coordinates for each horizontal station within circular arc segment. 

										// Calculate circle center, given start point and direction (angle between tangent and x-axis).
										double radiusDirection = 0.;
										is_CCW ? radiusDirection = startDirection + M_PI / 2 : radiusDirection = startDirection - M_PI / 2;
										double centerX = startCoords[0] + cos(radiusDirection) * radius;
										double centerY = startCoords[1] + sin(radiusDirection) * radius;
										double angleAlpha = atan2(startStationY - centerY, startStationX - centerX); // Angle between x-axis and vector(start, center).

										if(it_hor_stations.x_ != 0) // Skip calculation if x,y coordinate is already there.
										{
											double distanceStartToStation = it_hor_stations.distAlong_ - startStation.distAlong_; // Distance from start station to current station along alignment.

											if(is_CCW) {
												angleBeta = angleAlpha - distanceStartToStation / radius; // Angle between x-axis and current station according to circle direction.
											}
											else {
												angleBeta = angleAlpha + distanceStartToStation / radius; v// Angle between x-axis and current station according to circle direction.
											}
											it_hor_stations.x_ = centerX + radius * cos(angleBeta);
											it_hor_stations.y_ = centerY + radius * sin(angleBeta);
										} // end if 
									} // end stations iteration
								} // end if isCircArc

								// HORIZONTAL TRANSITION CURVE SEGMENT.
								if(segType == isTrans) {
									// Iterate over stations in transition curve segment.
									for(auto it_hor_stations = it_hor_segments.itHorizontal_->segmentStations_.begin()) {
										// (1) Get "station-specific" segment information.
										double startDirection = it_hor_stations.startDirection_;
										double distAlong = it_hor_stations.distAlong_; // Distance along of station.

									// (2) Get transition curve segment information.
										std::shared_ptr<typename IfcEntityTypesT::IfcTransitionCurveSegment2D> trans_curve_segment_2D =
											dynamic_pointer_cast<typename IfcEntityTypesT::IfcTransitionCurveSegment2D>(horCurveGeometry);

										// StartRadius type IfcLengthMeasure: if NIL, interpret as infinite (= no curvature) 
										if(!trans_curve_segment_2D->StartRadius) {
											BLUE_LOG(warning) << "IfcTransitionCurve: Start radius NIL, interpreted as infinite. (Segment ID: " << it_segment->getId() << ").";
											double startRadius = 0;
										}
										else {
											startRadius = trans_curve_segment_2D->StartRadius * length_factor;
										}
										// EndRadius type IfcLengthMeasure: if NIL, interpret as infinite (= no curvature)
										if(!trans_curve_segment_2D->EndRadius) {
											BLUE_LOG(warning) << "IfcTransitionCurve: End radius NIL, interpreted as infinite. (Segment ID: " << it_segment->getId() << ").";
											double endRadius = 0;
										}
										else {
											endRadius = trans_curve_segment_2D->EndRadius * length_factor;
										}
										// IsStartRadiusCCW type IfcBoolean
										if(!trans_curve_segment_2D->IsStartRadiusCCW) {
											BLUE_LOG(error) << "No direction information for start of IfcTransitionCurveSegment2D (counterclockwise/clockwise). (Segment ID: " << it_segment->getId() << ").";
											return;
										}
										bool is_start_ccw = trans_curve_segment_2D->IsStartRadiusCCW;
										// IsEndRadiusCCW type IfcBoolean
										if(!trans_curve_segment_2D->IsEndRadiusCCW) {
											BLUE_LOG(error) << "No direction information for end of IfcTransitionCurveSegment2D (counterclockwise/clockwise). (Segment ID: " << it_segment->getId() << ").";
											return;
										}
										bool is_end_ccw = trans_curve_segment_2D->IsEndRadiusCCW;
										// TransitionCurveType type IfcTransitionCurveType
										if(!trans_curve_segment_2D->TransitionCurveType) {
											BLUE_LOG(error) << "No curve type for IfcTransitionCurveSegment2D. (Segment ID: " << it_segment->getId() << ").";
											return;
										}
										enum trans_type = trans_curve_segment_2D->TransitionCurveType;	//trans curve types: http://www.buildingsmart-tech.org/ifc/IFC4x1/final/html/schema/ifcgeometryresource/lexical/ifctransitioncurvetype.htm

										// (3) Calculate x and y coordinates for each horizontal station within transition curve segment. 

										switch(trans_type) {
										case(BIQUADRATICPARABOLA):
										{
											if(it_hor_stations.x_ != 0) // Skip calculation if x,y coordinate is already there.
											{
												double x = distAlong; // x coordinate
												if(x <= segmentLength / 2) // y coordinate
												{
													double y = pow(x, 4) / (6 * endRadius * segmentLength * segmentLength);
												}
												else if(segmentLength / 2 < x && x <= segmentLength) {
													double y = -(pow(x, 4) / (6 * endRadius * pow(segmentLength, 2)) + (2 * pow(x, 3)) / (3 * endRadius * segmentLength)
														- pow(x, 2) / (2 * endRadius) + (segmentLength * x) / (6 * endRadius) - pow(segmentLength, 2) / (48 * endRadius));
												} // end elseif
											} // end endif
											it_hor_stations.x_ = x;
											it_hor_stations.y_ = y;
										} // end stations iteration.
										break;

										case(BLOSSCURVE):
										{
											// Skip calculation if x,y coordinate is already there.
											if(it_hor_stations.x_ != 0) {
												// Integration durch Substitution(s.Formel: http://www.buildingsmart-tech.org/ifc/IFC4x1/final/html/schema/ifcgeometryresource/lexical/ifctransitioncurvetype.htm).
												double teta_up = pow(distAlong + segmentLength, 3) / (endRadius * pow(segmentLength, 2)) - pow(distAlong + segmentLength, 4) / (2 * endRadius * pow(segmentLength, 3)); //values for upper boundary of integral
												double teta_low = pow(distAlong, 3) / (endRadius * pow(segmentLength, 2)) - pow(distAlong, 4) / (2 * endRadius * pow(segmentLength, 3)); //values for lower boundary of integral
												double teta_deriv_up = 2 * pow(distAlong + segmentLength, 2) / endRadius*pow(segmentLength, 2) - 4 * pow(distAlong + segmentLength, 3) / 2 * endRadius*pow(segmentLength, 3);
												double teta_deriv_low = 2 * pow(distAlong, 2) / endRadius*pow(segmentLength, 2) - 4 * pow(distAlong, 3) / 2 * endRadius*pow(segmentLength, 3);
												double x = sin(teta_up) / teta_deriv_up - sin(teta_low) / teta_deriv_low;
												double y = -cos(teta_up) / teta_deriv_up + cos(teta_low) / teta_deriv_low;
												it_hor_stations.x_ = x;
												it_hor_stations.y_ = y;
											}
										}
										break;

										case(CLOTHOIDCURVE):
										{
											it_hor_stations.x_ = distAlong*(1 - pow(distAlong, 4) / 40 * pow(endRadius, 2) * pow(segmentLength, 2) + pow(distAlong, 8) / 3456 * pow(endRadius, 4) * pow(segmentLength, 4));
											it_hor_stations.y_ = (pow(distAlong.3) / 6 * endRadius*segmentLength) * (1 - pow(distAlong, 4) / 56 * pow(endRadius, 2) * pow(segmentLength, 2) + pow(distAlong, 8) / 7040 * pow(endRadius, 4) * pow(segmentLength, 4));
										}
										break;

										case (COSINECURVE):
										{
											double psi = M_PI * distAlong / segmentLength;
											double x = distAlong
												- (pow(segmentLength, 2) / (8 * pow(M_PI, 2) * pow(endRadius, 2))) * segmentLength / M_PI
												* ((pow(psi, 3) / 3) + (psi / 2 - sin(psi) * cos(psi) / 2) - 2 * (sin(psi) - psi * cos(psi)));
											double y = segmentLength
												* (segmentLength / (2 * pow(M_PI, 2) * endRadius) * (pow(psi, 2) / 2 + cos(psi) - 1)
													- (pow(segmentLength, 3) / (48 * pow(M_PI, 4) * pow(endRadius, 3)))
													* (pow(psi, 4) / 4 + pow(sin(psi), 2) * cos(psi) / 3 - 16 * cos(psi) / 3 + 3 * pow(psi, 2) * cos(psi)
														- 6 * psi * sin(psi) + 3 * pow(psi, 2) / 4 - 3 * psi * sin(2 * psi) / 4 - 3 * cos(2 * psi) / 8 + 137 / 24));
											it_hor_stations.x_ = x;
											it_hor_stations.y_ = y;
										}
										break;

										case (CUBICPARABOLA):
										{
											double x = distAlong;
											double y = pow(x, 3) / (6 * endRadius * segmentLength);
											it_hor_stations.x_ = x;
											it_hor_stations.y_ = y;
										}
										break;

										/*TO DO: check math interpretation?*/
										case (SINECURVE):
										{
											double psi = (2 * M_PI * distAlong) / segmentLength;
											double x = distAlong*(1 - pow(segmentLength, 2) / (32 * pow(M_PI, 4)*pow(endRadius, 2) - (pow(segmentLength, 3) / 3840 * pow(M_PI, 5)*pow(endRadius, 2)))
												* (3 * pow(psi, 5) - 20 * pow(psi, 3) + 30 * psi - (240 - 60 * pow(psi, 2)*sin(psi) + 30 * cos(psi)*sin(psi) + 120 * psi*cos(psi))));
											// Integration durch Substitution (s. Formel: http://www.buildingsmart-tech.org/ifc/IFC4x1/final/html/schema/ifcgeometryresource/lexical/ifctransitioncurvetype.htm).
											double teta_up = pow((distAlong + segmentLength), 2) / (2 * endRadius*segmentLength) + (segmentLength / (4 * pow(M_PI, 2)*endRadius)) * (cos(2 * M_PI*(distAlong + segmentLength) / segmentLength) - 1);
											double teta_low = pow((distAlong + segmentLength), 2) / (2 * endRadius*segmentLength) + (segmentLength / (4 * pow(M_PI, 2)*endRadius)) * (cos(2 * M_PI*(distAlong) / segmentLength) - 1);
											double teta_deriv_up = 2 * (distAlong + segmentLength) / 2 * endRadius*segmentLength;
											double teta_deriv_low = 2 * distAlong / 2 * endRadius*segmentLength;
											double y = sin(teta_up) / teta_deriv_up - sin(teta_low) / teta_deriv_low;
											it_hor_stations.x_ = x;
											it_hor_stations.y_ = y;
										}
										break;

										} // end switch (trans_type)
									} // end if isTrans
								} // end iteration over horizontal stations

							} // end iteration over horizontal segments

							// VERTICAL.
							// Iterate over vertical segments and calculate missing z coordinates. 
							for(auto it_ver_segments : VerSegmentsVec) {

								std::shared_ptr<typename IfcEntityTypesT::IfcCurveSegment2D> horCurveGeometry =
									it_ver_segments.itVertical_->CurveGeometry;

								// Get "global" segment information necessary for calculation via pointer to segment. 
								double startStationX = it_ver_segments.itVertical_.segmentStations_[0]->x_;
								double startStationY = it_ver_segments.itVertical_.segmentStations_[0]->y_;
								enum SegType = it_ver_segments.itVertical_->segmentType_;
								double startDistAlong = it_ver_segments.itVertical_->segmentStations_.startDistAlong_;

								// VERTICAL LINE SEGMENT.
								if(segType == isLine) {
									// Iterate over stations in line segment.
									for(auto it_ver_stations = it_ver_segments.itVertical_->segmentStations_.begin()) {
										// (1) Get "station-specific" segment information.
										double startHeight = it_ver_stations.startHeight_;
										double startGradient = it_ver_stations.startGradient_;
										double horizontalLength = it_ver_stations.horizontalLength_;

										// (2) Get line segment information.
										// No additional information provided.

										// (3) Calculate x and y coordinates for each vertical station within line segment. 
										if(it_ver_stations.z_ != 0) // Skip calculation if z coordinate is already there.
										{
											double distanceToStartStation = horizontalLength - startDistAlong;
											it_ver_stations.z_ = startGradient * distanceToStartStation;
										}
									} // end station iteration
								} // endif isLine

								// VERTICAL CIRCULAR ARC SEGMENT.
								if(segType == isCircArc) {
									// Iterate over stations in circular arc segment.
									for(auto it_hor_stations = it_hor_segments.itHorizontal_->segmentStations_.begin()) {
										// (1) Get "station-specific" segment information.
										double startHeight = it_ver_stations.startHeight_;
										double startGradient = it_ver_stations.startGradient_;
										double horizontalLength = it_ver_stations.horizontalLength_;

										// (2) Get circular arc segment information.
										std::shared_ptr<typename IfcEntityTypesT::IfcAlignment2DVerSegCircularArc> v_seg_circ_arc_2D =
											dynamic_pointer_cast<typename IfcEntityTypesT::IfcAlignment2DVerSegCircularArc>(horCurveGeometry);

										// Radius type IfcPositiveLengthMeasure [1:1] 
										if(!v_seg_circ_arc_2D->Radius) {
											BLUE_LOG(error) << "No radius in IfcAlignment2DVerSegCircularArc\" (Segment ID : " << it_segment->getId() << ").";
											return;
										}
										double radius = v_seg_circ_arc_2D->Radius * length_factor;

										// IsConvex type IfcBoolean [1:1]
										if(!v_seg_circ_arc_2D->IsConvex) {
											BLUE_LOG(error) << "No curvature information in IfcAlignment2DVerSegCircularArc (convex/concave)\" (Segment ID : " << it_segment->getId() << ").";
											return;
										}
										bool is_convex = v_seg_circ_arc_2D->IsConvex;

										// (3) Calculate x and y coordinates for each vertical station within circular arc segment. 

										// Calculate circle centre.
										double distAlongCentre = startDistAlong - radius * startDistAlong / sqrt(1 + pow(startGradient, 2));
										double centreZ = startHeight + radius * startDistAlong / sqrt(1 + pow(startGradient, 2));

										// Calculate z coordinate according to isConvex
										if(it_ver_stations.z_ == 0) // Skip calculation if z coordinate is already there.
										{
											if(is_convex == true) {
												it_ver_stations.z_ = startDistAlong + sqrt(pow(radius, 2) - pow(horizontalLength - distAlongCentre, 2)); // Crest (decreasing gradiant)
											else
												it_ver_stations.z_ = startDistAlong - sqrt(pow(radius, 2) - pow(horizontalLength - distAlongCentre, 2)); // Sag (increasing gradiant)
											}
										} // end isConvex
									} // end if
								} // end isCircArc

								// PARABOLIC ARC SEGMENT
								if(segType == isParArc) {
									// Iterate over stations in parabolic arc segment.
									for(auto it_hor_stations = it_hor_segments.itHorizontal_->segmentStations_.begin()) {
										// (1) Get "station-specific" segment information.
										double startHeight = it_ver_stations.startHeight_;
										double startGradient = it_ver_stations.startGradient_;
										double horizontalLength = it_ver_stations.horizontalLength_;

										// (2) Get parabolic arc segment information.
										std::shared_ptr<typename IfcEntityTypesT::IfcAlignment2DVerSegParabolicArc> v_seg_par_arc_2D =
											dynamic_pointer_cast<typename IfcEntityTypesT::IfcAlignment2DVerSegParabolicArc>(horCurveGeometry);

										// ParabolaConstant type IfcPositiveLengthMeasure [1:1]
										if(!v_seg_par_arc_2D->ParabolaConstant) {
											BLUE_LOG(error) << "No parabola constant in IfcAlignment2DVerSegParabolicArc (Segment ID : " << it_segment->getId() << ").";
											return;
										}
										double arc_const = v_seg_par_arc_2D->ParabolaConstant * length_factor;

										// IsConvex type IfcBoolean [1:1]
										if(!v_seg_par_arc_2D->IsConvex) {
											BLUE_LOG(error) << "No curvature information in IfcAlignment2DVerSegParabolicArc (convex/concave) (Segment ID : " << it_segment->getId() << ").";
											return;
										}
										bool is_convex = v_seg_par_arc_2D->IsConvex;

										if(it_ver_stations.z_ != 0) { // Skip calculation if z coordinate is already there.
											double parabol_radius = is_convex == true ? arc_const : -arc_const;
											double parabol_grad = (horizontalLength - startDistAlong) / (parabol_radius + startGradient);
											it_ver_stations.z_ = (horizontalLength - startDistAlong) * (parabol_grad + startGradient) / 2 + startHeight;
										} // endif
									} // end vertical stations iteration
								} // end isParArc

							} // end iteration over vertical segments.

					// Step 3: Curve points vector with coordinates (x,y,z) for each station.

							// Create curve points vector to store coordinates (x,y,z) for each station.
							std::vector<carve::geom::vector<3> > curve_points;

							// Copy x,y,z information to curve_points vector by iterating over both lists of segments. 
							auto it_hor_seg = horSegments.begin();
							auto it_ver_seg = verSegments.begin();

							for(; it_hor_seg != horSegments.end() && it_ver_seg != verSegments.end();
								it_hor_seg++, it_ver_seg++) {

								auto it_hor_stations = it_hor_seg.segmentStations_.begin();
								auto it_ver_stations = it_ver_seg.segmentStations_.begin();

								for(; it_hor_stations != it_hor_seg.segmentStations_.end() && it_ver_stations != it_ver_seg.segmentStations_.end();
									it_hor_stations++, it_ver_stations++) {

									curve_points.push_back({ it_hor_stations.x_, it_hor_stations.y_, it_ver_stations.z_ });
								}
							} // end of both segments iteration

							// Add points to curve.
							segmentStartPoints.push_back(curve_points[0]);
							GeomUtils::appendPointsToCurve(curve_points, targetVec);

							return;

						} // end of IfcAlignmentCurve
					}

					// (2/6) IfcBSplineCurve SUBTYPE OF IfcBoundedCurve
					std::shared_ptr<typename IfcEntityTypesT::IfcBSplineCurve> bspline_curve =
						dynamic_pointer_cast<typename IfcEntityTypesT::IfcBSplineCurve>(bounded_curve);
					if(bspline_curve) {

								std::vector<std::shared_ptr<typename IfcEntityTypesT::IfcCartesianPoint>>& points =
									bspline_curve->ControlPointsList;

								std::vector<carve::geom::vector<3>> splinePoints;
								splinePoints.reserve(points.size());
								convertIfcCartesianPointVector(points, splinePoints);

								SplineConverterT<typename IfcEntityTypesT, IfcUnitConverterT>::convertIfcBSplineCurve(bspline_curve, splinePoints, targetVec);
								return;

								// TO DO: Implement IfcBSplineCurveWithKnots
								// TO DO: IfcRationalBSplineCurveWithKnots

							} // endif bsplinecurve

					// (3/6) IfcCompositeCurve SUBTYPE OF IfcBoundedCurve
					std::shared_ptr<typename IfcEntityTypesT::IfcCompositeCurve> composite_curve =
						dynamic_pointer_cast<typename IfcEntityTypesT::IfcCompositeCurve>(bounded_curve);
					if(composite_curve) {

								std::vector<std::shared_ptr<typename IfcEntityTypesT::IfcCompositeCurveSegment> > segments =
									composite_curve->Segments;

								std::vector<std::shared_ptr<typename IfcEntityTypesT::IfcCompositeCurveSegment> >::iterator it_segments =
									segments.begin();

								for(; it_segments != segments.end(); ++it_segments) {
									std::shared_ptr<typename IfcEntityTypesT::IfcCompositeCurveSegment> segment = (*it_segments);
									std::shared_ptr<typename IfcEntityTypesT::IfcCurve> segment_curve = segment->ParentCurve;

									std::vector<carve::geom::vector<3>> segment_vec;
									convertIfcCurve(segment_curve, segment_vec, segmentStartPoints);
									if(segment_vec.size() > 0) {
										GeomUtils::appendPointsToCurve(segment_vec, targetVec);
									}
								}
								return;
							} // endif composite curve

					// (4/6) IfcIndexedPolyline SUBTYPE OF IfcBoundedCurve
					std::shared_ptr<typename IfcEntityTypesT::IfcIndexedPolyline> index_poly_line =
						dynamic_pointer_cast<typename IfcEntityTypesT::IfcIndexedPolyline>(bounded_curve);
					if(index_poly_line)
							{
								// TO DO: implement
							}

					// (5/6) IfcPolyline SUBTYPE OF IfcBoundedCurve
					std::shared_ptr<typename IfcEntityTypesT::IfcPolyline> poly_line =
						dynamic_pointer_cast<typename IfcEntityTypesT::IfcPolyline>(bounded_curve);
					if(poly_line) {
								std::vector<std::shared_ptr<typename IfcEntityTypesT::IfcCartesianPoint> >& points = poly_line->Points;
								if(points.size() > 0) {
									convertIfcCartesianPointVector(points, targetVec);
									segmentStartPoints.push_back(carve::geom::VECTOR(
										points[0]->Coordinates[0] * length_factor,
										points[0]->Coordinates[1] * length_factor,
										0));
								}
								return;
							} // endif points

					// (6/6) IfcTrimmedCurve SUBTYPE OF IfcBoundedCurve
					std::shared_ptr<typename IfcEntityTypesT::IfcTrimmedCurve> trimmed_curve =
						dynamic_pointer_cast<typename IfcEntityTypesT::IfcTrimmedCurve>(bounded_curve);
					if(trimmed_curve) {
								std::shared_ptr<typename IfcEntityTypesT::IfcCurve> basis_curve = trimmed_curve->BasisCurve;
								std::vector<carve::geom::vector<3> > basis_curve_points;

								std::vector<std::shared_ptr<typename IfcEntityTypesT::IfcTrimmingSelect> > curve_trim1_vec =
									trimmed_curve->Trim1;
								std::vector<std::shared_ptr<typename IfcEntityTypesT::IfcTrimmingSelect> > curve_trim2_vec =
									trimmed_curve->Trim2;
								bool trimmed_sense_agreement = trimmed_curve->SenseAgreement;

								convertIfcCurve(basis_curve, basis_curve_points, segmentStartPoints,
									curve_trim1_vec, curve_trim2_vec, trimmed_sense_agreement);
								GeomUtils::appendPointsToCurve(basis_curve_points, targetVec);
								return;
							}

					} // end IfcBoundedCurve

				// ************************************************************************************************************************	//
				//	IfcConic SUPTYPE of IfcCurve																							//
				//	ABSTRACT SUPERTYPE of IfcCircle, IfcEllipse																				//
				// ************************************************************************************************************************	//
					std::shared_ptr<typename IfcEntityTypesT::IfcConic> conic =
						dynamic_pointer_cast<typename IfcEntityTypesT::IfcConic>(ifcCurve);
					if(conic) {
							
							std::shared_ptr<typename IfcEntityTypesT::IfcAxis2Placement> conic_placement = conic->Position;
							carve::math::Matrix conic_position_matrix(carve::math::Matrix::IDENT());

							std::shared_ptr<typename IfcEntityTypesT::IfcAxis2Placement2D> axis2placement2d =
								dynamic_pointer_cast<typename IfcEntityTypesT::IfcAxis2Placement2D>(conic_placement);

							if(axis2placement2d) {
								PlacementConverterT<IfcEntityTypesT>::convertIfcAxis2Placement2D(axis2placement2d,
									conic_position_matrix, length_factor);
							}
							else if(dynamic_pointer_cast<typename IfcEntityTypesT::IfcAxis2Placement3D>(conic_placement)) {
								std::shared_ptr<typename IfcEntityTypesT::IfcAxis2Placement3D> axis2placement3d =
									dynamic_pointer_cast<typename IfcEntityTypesT::IfcAxis2Placement3D>(conic_placement);

								PlacementConverterT<IfcEntityTypesT>::convertIfcAxis2Placement3D(axis2placement3d,
									conic_position_matrix, length_factor);
							}

							// (1/2) IfcCircle SUBTYPE OF IfcConic
							std::shared_ptr<typename IfcEntityTypesT::IfcCircle> circle =
								dynamic_pointer_cast<typename IfcEntityTypesT::IfcCircle>(conic);
							if(circle) {

								// Get radius
								double circle_radius = 0.0;
								if(circle->Radius) {
									circle_radius = circle->Radius * length_factor; 
								}

								carve::geom::vector<3> circle_center =
									conic_position_matrix*carve::geom::VECTOR(0, 0, 0);

								double trim_angle1 = 0.0;
								double trim_angle2 = M_PI*2.0;

								// Check for trimming begin
								std::shared_ptr<typename IfcEntityTypesT::IfcParameterValue> trim_par1;
								if(trim1Vec.size() > 0) {
									if(OpenInfraPlatform::ExpressBinding::findFirstInVector<typename IfcEntityTypesT::IfcParameterValue, typename IfcEntityTypesT::IfcTrimmingSelect>(trim1Vec, trim_par1)) {
										trim_angle1 = trim_par1 * plane_angle_factor;
									}
									else {
										std::shared_ptr<typename IfcEntityTypesT::IfcCartesianPoint> trim_point1;
										if(OpenInfraPlatform::ExpressBinding::findFirstInVector<typename IfcEntityTypesT::IfcCartesianPoint, typename IfcEntityTypesT::IfcTrimmingSelect>(trim1Vec, trim_point1)) {
											carve::geom::vector<3> trim_point;
											convertIfcCartesianPoint(trim_point1, trim_point);

											trim_angle1 = getAngleOnCircle(circle_center,
												circle_radius,
												trim_point);
										}
									}
								}

								if(trim2Vec.size() > 0) {
									// check for trimming end
									std::shared_ptr<typename IfcEntityTypesT::IfcParameterValue> trim_par2;
									if(OpenInfraPlatform::ExpressBinding::findFirstInVector<typename IfcEntityTypesT::IfcParameterValue, typename IfcEntityTypesT::IfcTrimmingSelect>(trim2Vec, trim_par2)) {
										trim_angle2 = trim_par2  * plane_angle_factor;
									}
									else {
										std::shared_ptr<typename IfcEntityTypesT::IfcCartesianPoint> ifc_trim_point;
										if(OpenInfraPlatform::ExpressBinding::findFirstInVector<typename IfcEntityTypesT::IfcCartesianPoint, typename IfcEntityTypesT::IfcTrimmingSelect>(trim2Vec, ifc_trim_point)) {
											carve::geom::vector<3> trim_point;
											convertIfcCartesianPoint(ifc_trim_point, trim_point);
											trim_angle2 = getAngleOnCircle(circle_center,
												circle_radius,
												trim_point);
										}
									}
								}

								double start_angle = trim_angle1;
								double opening_angle = 0;

								if(senseAgreement) {
									if(trim_angle1 < trim_angle2) {
										opening_angle = trim_angle2 - trim_angle1;
									}
									else {
										// circle passes 0 angle
										opening_angle = trim_angle2 - trim_angle1 + 2.0*M_PI;
									}
								}
								else {
									if(trim_angle1 > trim_angle2) {
										opening_angle = trim_angle2 - trim_angle1;
									}
									else {
										// circle passes 0 angle
										opening_angle = trim_angle2 - trim_angle1 - 2.0*M_PI;
									}
								}

								if(opening_angle > 0) {
									while(opening_angle > 2.0*M_PI) {
										opening_angle -= 2.0*M_PI;
									}
								}
								else {
									while(opening_angle < -2.0*M_PI) {
										opening_angle += 2.0*M_PI;
									}
								}

								int num_segments = geomSettings->num_vertices_per_circle
									* (abs(opening_angle) / (2.0*M_PI));

								if(num_segments < geomSettings->min_num_vertices_per_arc) {
									num_segments = geomSettings->min_num_vertices_per_arc;
								}

								const double circle_center_x = 0.0;
								const double circle_center_y = 0.0;
								std::vector<carve::geom::vector<2> > circle_points;
								ProfileConverterT<IfcEntityTypesT, IfcUnitConverterT>::addArcWithEndPoint(circle_points, circle_radius, start_angle,
									opening_angle, circle_center_x,
									circle_center_y, num_segments);

								if(circle_points.size() > 0) {
									// apply position
									for(unsigned int i = 0; i < circle_points.size(); ++i) {
										carve::geom::vector<2>&  point = circle_points.at(i);
										carve::geom::vector<3> point3d(carve::geom::VECTOR(point.x, point.y, 0));
										point3d = conic_position_matrix * point3d;
										point.x = point3d.x;
										point.y = point3d.y;
									}

									GeomUtils::appendPointsToCurve(circle_points, targetVec);
									segmentStartPoints.push_back(carve::geom::VECTOR(circle_points.at(0).x,
										circle_points.at(0).y,
										0));
								}

								return;
							}

							// (2/2) IfcEllipse SUBTYPE OF IfcConic

							std::shared_ptr<typename IfcEntityTypesT::IfcEllipse> ellipse =
								dynamic_pointer_cast<typename IfcEntityTypesT::IfcEllipse>(conic);		
							if(ellipse) {
								if(ellipse->SemiAxis1) {
									if(ellipse->SemiAxis2) {

										double xRadius = ellipse->SemiAxis1 * length_factor;
										double yRadius = ellipse->SemiAxis2 * length_factor;

										double radiusMax = std::max(xRadius, yRadius);
										int num_segments = geomSettings->num_vertices_per_circle;
										// TODO: adapt to model size and complexity

										// todo: implement clipping

										std::vector<carve::geom::vector<3> > circle_points;
										double angle = 0;
										for(int i = 0; i < num_segments; ++i) {
											circle_points.push_back(carve::geom::vector<3>(
												carve::geom::VECTOR(xRadius * cos(angle),
													yRadius * sin(angle),
													0)));
											angle += 2.0*M_PI / double(num_segments);
										}

										// apply position
										for(unsigned int i = 0; i < circle_points.size(); ++i) {
											carve::geom::vector<3>& point = circle_points.at(i);
											point = conic_position_matrix * point;
										}
										GeomUtils::appendPointsToCurve(circle_points, targetVec);

										//if( segmentStartPoints != NULL )
										{
											carve::geom::vector<3> pt0 = circle_points.at(0);
											segmentStartPoints.push_back(pt0);
										}
									}
								}
								return;
							}

						}

				// ************************************************************************************************************************	//
				//	IfcLine SUPTYPE of IfcCurve																								//
				// ************************************************************************************************************************	//
					std::shared_ptr<typename IfcEntityTypesT::IfcLine> line =
						dynamic_pointer_cast<typename IfcEntityTypesT::IfcLine>(ifcCurve);
					if(line) {

						// Part 1: Get information from IfcLine. 

							// Get IfcLine attributes: line point and line direction. 
							std::shared_ptr<typename IfcEntityTypesT::IfcCartesianPoint> ifc_line_point = line->Pnt;
							carve::geom::vector<3> line_origin;
							convertIfcCartesianPoint(ifc_line_point, line_origin);

							std::shared_ptr<typename IfcEntityTypesT::IfcVector> line_vec = line->Dir;
							if(!line_vec) {
								return;
							}
								// Get IfcVector attributes: line orientation and magnitude. 

								// Orientation type IfcDirection
								double ifc_line_direction = line_vec->Orientation;

									// Get IfcDirection attribute: direction ratios. 
									std::vector<double> direction_ratios(ifc_line_direction->DirectionRatios.size());
									for(int i = 0; i < direction_ratios.size(); ++i) {
										direction_ratios[i] = ifc_line_direction->DirectionRatios[i];
									}

									carve::geom::vector<3> line_direction;
									if(direction_ratios.size() > 1) {
										if(direction_ratios.size() > 2) {
											line_direction = carve::geom::VECTOR(direction_ratios[0],
												direction_ratios[1],
												direction_ratios[2]);
										}
										else {
											line_direction = carve::geom::VECTOR(direction_ratios[0],
												direction_ratios[1], 0);
										}
									}
									line_direction.normalize();

								// Magnitude type IfcLengthMeasure
								double line_magnitude = line_vec->Magnitude * length_factor;

						// Part 2: Trimming
								
							// Check for trimming at beginning of line
							double start_parameter = 0.0;
							std::shared_ptr<typename IfcEntityTypesT::IfcParameterValue> trim_par1;
							if(OpenInfraPlatform::ExpressBinding::findFirstInVector<typename IfcEntityTypesT::IfcParameterValue, typename IfcEntityTypesT::IfcTrimmingSelect>(trim1Vec, trim_par1)) {
								start_parameter = trim_par1;
								line_origin = line_origin + line_direction * start_parameter;
							}
							else {
								std::shared_ptr<typename IfcEntityTypesT::IfcCartesianPoint> ifc_trim_point;
								if(OpenInfraPlatform::ExpressBinding::findFirstInVector<typename IfcEntityTypesT::IfcCartesianPoint, typename IfcEntityTypesT::IfcTrimmingSelect>(trim1Vec, ifc_trim_point)) {
									carve::geom::vector<3> trim_point;
									convertIfcCartesianPoint(ifc_trim_point, trim_point);

									carve::geom::vector<3> closest_point_on_line;
									GeomUtils::closestPointOnLine(trim_point, line_origin,
										line_direction, closest_point_on_line);

									if((closest_point_on_line - trim_point).length() < 0.0001) {
										// trimming point is on the line
										line_origin = trim_point;
									}
								}
							}

							// Check for trimming at end of line
							carve::geom::vector<3> line_end;
							std::shared_ptr<typename IfcEntityTypesT::IfcParameterValue> trim_par2;
							if(OpenInfraPlatform::ExpressBinding::findFirstInVector<typename IfcEntityTypesT::IfcParameterValue, typename IfcEntityTypesT::IfcTrimmingSelect>(trim2Vec, trim_par2)) {
								line_magnitude_value = trim_par2 * length_factor;
								line_end = line_origin + line_direction * line_magnitude;
							}
							else {
								std::shared_ptr<typename IfcEntityTypesT::IfcCartesianPoint> ifc_trim_point;
								if(OpenInfraPlatform::ExpressBinding::findFirstInVector<typename IfcEntityTypesT::IfcCartesianPoint, typename IfcEntityTypesT::IfcTrimmingSelect>(trim2Vec, ifc_trim_point)) {
									carve::geom::vector<3> trim_point;
									convertIfcCartesianPoint(ifc_trim_point, trim_point);

									carve::geom::vector<3> closest_point_on_line;
									GeomUtils::closestPointOnLine(trim_point, line_origin,
										line_direction, closest_point_on_line);

									if((closest_point_on_line - trim_point).length() < 0.0001) {
										// trimming point is on the line
										line_end = trim_point;
									}
								}
							}

						// Part 3: Add line points
							std::vector<carve::geom::vector<3> > points_vec;
							points_vec.push_back(line_origin);
							points_vec.push_back(line_end);

							GeomUtils::appendPointsToCurve(points_vec, targetVec);

							// if( segment_start_points != NULL )
							{
								segmentStartPoints.push_back(line_origin);
							}
							return;
						}

				// ************************************************************************************************************************	//
				//	IfcOffsetCurve SUPTYPE of IfcCurve																						//
				//	ABSTRACT SUPERTYPE OF IfcOffsetCurve2D, IfcOffsetCurve3D, IfcOffsetCurveByDistances										//
				// ************************************************************************************************************************	//
					std::shared_ptr<typename IfcEntityTypesT::IfcOffsetCurve> offset_curve =
						dynamic_pointer_cast<typename IfcEntityTypesT::IfcOffsetCurve>(ifcCurve);
					if(offset_curve) {

							// (1/3) IfcOffsetCurve2D SUBTYPE OF IfcOffsetCurve
							std::shared_ptr<typename IfcEntityTypesT::IfcOffsetCurve2D> offset_curve_2d =
								dynamic_pointer_cast<typename IfcEntityTypesT::IfcOffsetCurve2D>(offset_curve);
							if(offset_curve_2d) {
								#ifdef _DEBUG
								std::cout << "Warning\t| IfcOffsetCurve2D not implemented" << std::endl;
								#endif
								return;
							}

							// (2/3) IfcOffsetCurve3D SUBTYPE OF IfcOffsetCurve
							std::shared_ptr<typename IfcEntityTypesT::IfcOffsetCurve3D> offset_curve_3d =
								dynamic_pointer_cast<typename IfcEntityTypesT::IfcOffsetCurve3D>(offset_curve);
							if(offset_curve_3d) {
#ifdef _DEBUG
								std::cout << "Warning\t| IfcOffsetCurve3D not implemented" << std::endl;
#endif
								return;
							}

							// (3/3) IfcOffsetCurveByDistances SUBTYPE OF IfcOffsetCurve
							std::shared_ptr<typename IfcEntityTypesT::IfcOffsetCurveByDistances> offset_curve_dist =
								dynamic_pointer_cast<typename IfcEntityTypesT::IfcOffsetCurveByDistances>(offset_curve);
							if(offset_curve_dist) {
								#ifdef _DEBUG
								std::cout << "Warning\t| IfcOffsetCurveByDistances not implemented" << std::endl;
								#endif
								return;
							}
						}

				// ************************************************************************************************************************	//
				//	IfcPcurve SUPTYPE of IfcCurve																							//
				// ************************************************************************************************************************	//
					std::shared_ptr<typename IfcEntityTypesT::IfcPcurve> p_curve =
						dynamic_pointer_cast<typename IfcEntityTypesT::IfcPcurve>(ifcCurve);
					if(p_curve) 						
					{
						// TO DO: implement
					}

				// ************************************************************************************************************************	//
				//	IfcSurfaceCurve SUPTYPE of IfcCurve																						//
				//	ABSTRACT SUPERTYPE OF IfcIntersectionCurve, IfcSeamCurve																//
				// ************************************************************************************************************************	//
					std::shared_ptr<typename IfcEntityTypesT::IfcSurfaceCurve> surface_curve =
						dynamic_pointer_cast<typename IfcEntityTypesT::IfcSurfaceCurve>(ifcCurve);
					if(surface_curve) 
					{
						// (1/2) IfcIntersectionCurve SUBTYPE OF IfcSurfaceCurve
						std::shared_ptr<typename IfcEntityTypesT::IfcIntersectionCurve> intersection_curve =
							dynamic_pointer_cast<typename IfcEntityTypesT::IfcIntersectionCurve>(ifcCurve);
						if(intersection_curve)
						{
							// TO DO: implement
						}

						// (2/2) IfcSeamCurve SUBTYPE OF IfcSurfaceCurve
						std::shared_ptr<typename IfcEntityTypesT::IfcSeamCurve> seam_curve =
							dynamic_pointer_cast<typename IfcEntityTypesT::IfcSeamCurve>(ifcCurve);
						if(seam_curve)
						{
							// TO DO: implement
						}

					} // endif surface curve

				// ************************************************************************************************************************	//
				//	Functions			
				//	convertIfcCurve2D, convertIfcCartesianPoint (+ related functions), getAngleOnCircle
				// ************************************************************************************************************************	//

				// Function 1: Convert 2D IfcCurve (overloaded). 

				void convertIfcCurve2D(const std::shared_ptr<typename IfcEntityTypesT::IfcCurve>& ifcCurve,
					std::vector<carve::geom::vector<2>>& loops,
					std::vector<carve::geom::vector<2>>& segmentStartPoints) const
				{
					std::vector<std::shared_ptr<typename IfcEntityTypesT::IfcTrimmingSelect>> trim1Vec;
					std::vector<std::shared_ptr<typename IfcEntityTypesT::IfcTrimmingSelect>> trim2Vec;
					convertIfcCurve2D(ifcCurve, loops, segmentStartPoints, trim1Vec, trim2Vec, true);
				}

				void convertIfcCurve2D(const std::shared_ptr<typename IfcEntityTypesT::IfcCurve>& ifcCurve,
					std::vector<carve::geom::vector<2>>& targetVec,
					std::vector<carve::geom::vector<2>>& segmentStartPoints,
					std::vector<std::shared_ptr<typename IfcEntityTypesT::IfcTrimmingSelect>>& trim1Vec,
					std::vector<std::shared_ptr<typename IfcEntityTypesT::IfcTrimmingSelect>>& trim2Vec,
					bool senseAgreement) const
				{
					std::vector<carve::geom::vector<3>> target_vec_3d;
					std::vector<carve::geom::vector<3>> segment_start_points_3d;

					convertIfcCurve(ifcCurve, target_vec_3d, segment_start_points_3d,
						trim1Vec, trim2Vec, senseAgreement);

					for(int i = 0; i < target_vec_3d.size(); ++i) {
						carve::geom::vector<3>& point_3d = target_vec_3d[i];
						targetVec.push_back(carve::geom::VECTOR(point_3d.x, point_3d.y));
					}
					for(int i = 0; i < segment_start_points_3d.size(); ++i) {
						carve::geom::vector<3>& point_3d = segment_start_points_3d[i];
						segmentStartPoints.push_back(carve::geom::VECTOR(point_3d.x, point_3d.y));
					}
				}

				// Function 2a: Convert IfcPolyline.
				void convertIfcPolyline(const std::shared_ptr<typename IfcEntityTypesT::IfcPolyline>& polyLine,
					std::vector<carve::geom::vector<3>>& loop) const
				{
					convertIfcCartesianPointVector(polyLine->Points, loop);
				}

				// Function 2b: Convert IfcLoop.
				void convertIfcLoop(const std::shared_ptr<typename IfcEntityTypesT::IfcLoop>& loop,
					std::vector<carve::geom::vector<3>>& loopPoints) const
				{
					const std::shared_ptr<typename IfcEntityTypesT::IfcPolyLoop> polyLoop =
						dynamic_pointer_cast<typename IfcEntityTypesT::IfcPolyLoop>(loop);

					if(polyLoop) {
						const std::vector<std::shared_ptr<typename IfcEntityTypesT::IfcCartesianPoint>>& ifcPoints = polyLoop->Polygon;
						convertIfcCartesianPointVectorSkipDuplicates(ifcPoints, loopPoints);

						// If first and last point have same coordinates, remove last point
						while(loopPoints.size() > 2) {
							carve::geom3d::Vector& first = loopPoints.front();
							carve::geom3d::Vector& last = loopPoints.back();

							if(abs(first.x - last.x) < 0.00000001) {
								if(abs(first.y - last.y) < 0.00000001) {
									if(abs(first.z - last.z) < 0.00000001) {
										loopPoints.pop_back();
										continue;
									}
								}
							}
							break;
						}
						return;
					}

					std::shared_ptr<typename IfcEntityTypesT::IfcEdgeLoop> edgeLoop =
						dynamic_pointer_cast<typename IfcEntityTypesT::IfcEdgeLoop>(loop);

					if(edgeLoop) {
						std::vector<std::shared_ptr<typename IfcEntityTypesT::IfcOrientedEdge>>& edgeList = edgeLoop->EdgeList;

						// go through every edge in the edge list
						for(auto it_edge = edgeList.begin(); it_edge != edgeList.end(); ++it_edge) {
							// edge loop consists of many oriented edges
							std::shared_ptr<typename IfcEntityTypesT::IfcOrientedEdge> orientedEdge = (*it_edge);
							// which are described by the type of its edge element object
							std::shared_ptr<typename IfcEntityTypesT::IfcEdge>& edgeElement = orientedEdge->EdgeElement;

							std::shared_ptr<typename IfcEntityTypesT::IfcEdgeCurve> edgeCurve =
								std::dynamic_pointer_cast<typename IfcEntityTypesT::IfcEdgeCurve>(edgeElement);

							if(edgeCurve) {
								std::shared_ptr<typename IfcEntityTypesT::IfcCurve>& curveGeom = edgeCurve->EdgeGeometry;
								std::vector<carve::geom::vector<3>> segmentStartPoints;

								convertIfcCurve(curveGeom, loopPoints, segmentStartPoints);

								continue;
							}

							std::shared_ptr<typename IfcEntityTypesT::IfcSubedge> subEdge =
								std::dynamic_pointer_cast<typename IfcEntityTypesT::IfcSubedge>(edgeElement);

							if(subEdge) {
								std::cout << "ERROR\t| IfcSubedge not implemented" << std::endl;
								continue;
							}

							std::cout << "ERROR\t| Entity " << orientedEdge->classname() << " not handled" << std::endl;

							// every edge consists of one start and end vertex
							std::shared_ptr<typename IfcEntityTypesT::IfcVertex>& edgeStartVertex = edgeElement->EdgeStart;
							std::shared_ptr<typename IfcEntityTypesT::IfcVertexPoint> edgeStartVertexPoint =
								dynamic_pointer_cast<typename IfcEntityTypesT::IfcVertexPoint>(edgeStartVertex);
							
							if (edgeStartVertexPoint)
							{
								if (edgeStartVertexPoint->VertexGeometry)
								{
									std::shared_ptr<typename IfcEntityTypesT::IfcPoint>& startPoint =
										edgeStartVertexPoint->VertexGeometry;
									std::shared_ptr<typename IfcEntityTypesT::IfcCartesianPoint> ifcPoint =
										dynamic_pointer_cast<typename IfcEntityTypesT::IfcCartesianPoint>(startPoint);
									if (!ifcPoint)
									{
										// TODO: could be also  IfcPointOnCurve, IfcPointOnSurface
							/			continue;
									}
									// TODO: implement
								}
							}

						}
					}
				}

				// Function 2c: Convert IfcCartesianPoint (overloaded). 
				void convertIfcCartesianPoint(const std::shared_ptr<typename IfcEntityTypesT::IfcCartesianPoint>& ifcPoint,
					carve::geom::vector<3> & point) const
				{
					double length_factor = unitConverter->getLengthInMeterFactor();

					if(ifcPoint->Coordinates.size() > 2) {
						double x = ifcPoint->Coordinates[0] * length_factor;
						double y = ifcPoint->Coordinates[1] * length_factor;
						double z = ifcPoint->Coordinates[2] * length_factor;

						point = carve::geom::VECTOR(x, y, z);
					}
					else if(ifcPoint->Coordinatese() > 1) {
						double x = ifcPoint->Coordinates[0] * length_factor;
						double y = ifcPoint->Coordinates[1] * length_factor;

						point = carve::geom::VECTOR(x, y, 0.0);
					}
				}

				static void convertIfcCartesianPoint(const std::shared_ptr<typename IfcEntityTypesT::IfcCartesianPoint>& ifcPoint,
					carve::geom::vector<3> & point,
					double lengthFactor)
				{
					if(ifcPoint->Coordinates.size() > 2) {
						double x = ifcPoint->Coordinates[0] * lengthFactor;
						double y = ifcPoint->Coordinates[1] * lengthFactor;
						double z = ifcPoint->Coordinates[2] * lengthFactor;

						point = carve::geom::VECTOR(x, y, z);
					}
					else if(ifcPoint->Coordinates.size() > 1) {
						double x = ifcPoint->Coordinates[0] * lengthFactor;
						double y = ifcPoint->Coordinates[1] * lengthFactor;

						point = carve::geom::VECTOR(x, y, 0.0);
					}
				}

				// Function 2d: Convert IfcCartesianPoint vector.
				void convertIfcCartesianPointVector(
					const std::vector<std::shared_ptr<typename IfcEntityTypesT::IfcCartesianPoint>>& points,
					std::vector<carve::geom::vector<3>>& loop) const
				{
					const double length_factor = unitConverter->getLengthInMeterFactor();
					const unsigned int num_points = points.size();

					for(unsigned int i_point = 0; i_point < num_points; ++i_point)  
					{
						if(points[i_point]->Coordinates.size() > 2) {
							double x = points[i_point]->Coordinates[0] * length_factor;
							double y = points[i_point]->Coordinates[1] * length_factor;
							double z = points[i_point]->Coordinates[2] * length_factor;

							loop.push_back(carve::geom::VECTOR(x, y, z));
						}
						else if(points[i_point]->Coordinates.size() > 1) {

							double x = points[i_point]->Coordinates[0] * length_factor;
							double y = points[i_point]->Coordinates[1] * length_factor;

							loop.push_back(carve::geom::VECTOR(x, y, 0.0));
						}
						else {
							std::cout << "convertIfcCartesianPointVector: ifc_pt->Coordinates.size() != 2"
								<< std::endl;
						}
					}
				}

				// Function 2e: Convert IfcCartesianPoint vector and skip duplicates.
				void convertIfcCartesianPointVectorSkipDuplicates(
					const std::vector<std::shared_ptr<typename IfcEntityTypesT::IfcCartesianPoint> >& ifcPoints,
					std::vector<carve::geom::vector<3> >& loop) const
				{
					const double length_factor = unitConverter->getLengthInMeterFactor();
					std::vector<std::shared_ptr<typename IfcEntityTypesT::IfcCartesianPoint> >::const_iterator it_cp; // iterates over points
					int i = 0;
					carve::geom::vector<3>  vertex_previous;

					for(it_cp = ifcPoints.begin(); it_cp != ifcPoints.end(); ++it_cp, ++i) {
						std::shared_ptr<typename IfcEntityTypesT::IfcCartesianPoint> cp = (*it_cp);
						const int cp_id = cp->getId();

						double x = 0.0, y = 0.0, z = 0.0;

						if(cp->Coordinates.size() > 2) {
							x = cp->Coordinates[0] * length_factor;
							y = cp->Coordinates[1] * length_factor;
							z = cp->Coordinates[2] * length_factor;
						}
						else if(cp->Coordinates.size() > 1) {

							x = cp->Coordinates[0] * length_factor;
							y = cp->Coordinates[1] * length_factor;
						}

						carve::geom::vector<3>  vertex(carve::geom::VECTOR(x, y, z));

						// skip duplicate vertices
						if(it_cp != ifcPoints.begin()) {
							if(abs(vertex.x - vertex_previous.x) < 0.00000001) {
								if(abs(vertex.y - vertex_previous.y) < 0.00000001) {
									if(abs(vertex.z - vertex_previous.z) < 0.00000001) {
										// TODO: is it better to report degenerated loops, or to just omit them?
										continue;
									}
								}
							}
						}
						loop.push_back(vertex);
						vertex_previous = vertex;
					}
				}

				// Function 3: Get angle on circle (returns angle if the given point lies on the circle; if not, -1 is returned). 
				static double getAngleOnCircle(const carve::geom::vector<3>& circleCenter,
					double circleRadius,
					const carve::geom::vector<3>& trimPoint)
				{
					double result_angle = -1.0;
					carve::geom::vector<3> center_trim_point = trimPoint - circleCenter;
					if(abs(center_trim_point.length() - circleRadius) < 0.0001) {
						carve::geom::vector<3> center_trim_point_direction = center_trim_point;
						center_trim_point_direction.normalize();
						double cos_angle = carve::geom::dot(center_trim_point_direction, carve::geom::vector<3>(carve::geom::VECTOR(1.0, 0, 0)));

						if(abs(cos_angle) < 0.0001) {
							if(center_trim_point.y > 0) {
								result_angle = M_PI_2;
							}
							else if(center_trim_point.y < 0) {
								result_angle = M_PI*1.5;
							}
						}
						else {
							if(center_trim_point.y > 0) {
								result_angle = acos(cos_angle);
							}
							else if(center_trim_point.y < 0) {
								result_angle = 2.0*M_PI - acos(cos_angle);
							}
						}
					}
					return result_angle;
				}

			protected:
				std::shared_ptr<GeometrySettings>	geomSettings;
				std::shared_ptr<IfcUnitConverterT>	unitConverter;
		};
	}
};
#endif
