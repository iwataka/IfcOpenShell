﻿/********************************************************************************
 *                                                                              *
 * This file is part of IfcOpenShell.                                           *
 *                                                                              *
 * IfcOpenShell is free software: you can redistribute it and/or modify         *
 * it under the terms of the Lesser GNU General Public License as published by  *
 * the Free Software Foundation, either version 3.0 of the License, or          *
 * (at your option) any later version.                                          *
 *                                                                              *
 * IfcOpenShell is distributed in the hope that it will be useful,              *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of               *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                 *
 * Lesser GNU General Public License for more details.                          *
 *                                                                              *
 * You should have received a copy of the Lesser GNU General Public License     *
 * along with this program. If not, see <http://www.gnu.org/licenses/>.         *
 *                                                                              *
 ********************************************************************************/

/********************************************************************************
 *                                                                              *
 * Implementations of the various conversion functions defined in IfcRegister.h *
 *                                                                              *
 ********************************************************************************/

#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt2d.hxx>
#include <gp_Vec2d.hxx>
#include <gp_Dir2d.hxx>
#include <gp_Mat.hxx>
#include <gp_Mat2d.hxx>
#include <gp_GTrsf.hxx>
#include <gp_GTrsf2d.hxx>
#include <gp_Trsf.hxx>
#include <gp_Trsf2d.hxx>
#include <gp_Ax3.hxx>
#include <gp_Ax2d.hxx>
#include <gp_Pln.hxx>
#include <gp_Circ.hxx>

#include <TColgp_Array1OfPnt.hxx>
#include <TColgp_Array1OfPnt2d.hxx>
#include <TColStd_Array1OfReal.hxx>
#include <TColStd_Array1OfInteger.hxx>
#include <Geom_Line.hxx>
#include <Geom_Circle.hxx>
#include <Geom_Ellipse.hxx>
#include <Geom_TrimmedCurve.hxx>

#include <BRepOffsetAPI_Sewing.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>

#include <TopoDS.hxx>
#include <TopoDS_Wire.hxx>
#include <TopoDS_Face.hxx>
#include <TopExp_Explorer.hxx>

#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepBuilderAPI_MakeShell.hxx>
#include <BRepBuilderAPI_MakeSolid.hxx>
#include <BRepPrimAPI_MakeHalfSpace.hxx>
#include <BRepAlgoAPI_Cut.hxx>

#include <ShapeFix_Shape.hxx>
#include <ShapeFix_ShapeTolerance.hxx>
#include <ShapeFix_Solid.hxx>

#include <TopLoc_Location.hxx>

#include <BRepCheck_Analyzer.hxx>
#include <BRepAlgoAPI_Common.hxx>

#include <BRepClass3d_SolidClassifier.hxx>

#include "../ifcgeom/IfcGeom.h"

bool IfcGeom::convert(const Ifc2x3::IfcExtrudedAreaSolid::ptr l, TopoDS_Shape& shape) {
	TopoDS_Face face;
	if ( ! IfcGeom::convert_face(l->SweptArea(),face) ) return false;
	const double height = l->Depth() * IfcGeom::GetValue(GV_LENGTH_UNIT);
	gp_Trsf trsf;
	IfcGeom::convert(l->Position(),trsf);

	gp_Dir dir;
	convert(l->ExtrudedDirection(),dir);

	shape = BRepPrimAPI_MakePrism(face,height*dir);
	shape.Move(trsf);
	return ! shape.IsNull();
}
bool IfcGeom::convert(const Ifc2x3::IfcFacetedBrep::ptr l, IfcRepresentationShapeItems& shape) {
	TopoDS_Shape s;
	const SurfaceStyle* collective_style = get_style(l);
	if (IfcGeom::convert_shape(l->Outer(),s) ) {
		const SurfaceStyle* indiv_style = get_style(l->Outer());
		shape.push_back(IfcRepresentationShapeItem(s, indiv_style ? indiv_style : collective_style));
		return true;
	}
	return false;
}
bool IfcGeom::convert(const Ifc2x3::IfcFaceBasedSurfaceModel::ptr l, IfcRepresentationShapeItems& shapes) {
	Ifc2x3::IfcConnectedFaceSet::list facesets = l->FbsmFaces();
	const SurfaceStyle* collective_style = get_style(l);
	for( Ifc2x3::IfcConnectedFaceSet::it it = facesets->begin(); it != facesets->end(); ++ it ) {
		TopoDS_Shape s;
		const SurfaceStyle* shell_style = get_style(*it);
		if (IfcGeom::convert_shape(*it,s)) {
			shapes.push_back(IfcRepresentationShapeItem(s, shell_style ? shell_style : collective_style));
		}
	}
	return true;
}
bool IfcGeom::convert(const Ifc2x3::IfcHalfSpaceSolid::ptr l, TopoDS_Shape& shape) {
	Ifc2x3::IfcSurface::ptr surface = l->BaseSurface();
	if ( ! surface->is(Ifc2x3::Type::IfcPlane) ) {
		Logger::Message(Logger::LOG_ERROR, "Unsupported BaseSurface:", surface->entity);
		return false;
	}
	gp_Pln pln;
	IfcGeom::convert(reinterpret_pointer_cast<Ifc2x3::IfcSurface,Ifc2x3::IfcPlane>(surface),pln);
	const gp_Pnt pnt = pln.Location().Translated( l->AgreementFlag() ? -pln.Axis().Direction() : pln.Axis().Direction());
	shape = BRepPrimAPI_MakeHalfSpace(BRepBuilderAPI_MakeFace(pln),pnt).Solid();
	return true;
}
bool IfcGeom::convert(const Ifc2x3::IfcPolygonalBoundedHalfSpace::ptr l, TopoDS_Shape& shape) {
	TopoDS_Shape halfspace;
	if ( ! IfcGeom::convert(reinterpret_pointer_cast<Ifc2x3::IfcPolygonalBoundedHalfSpace,Ifc2x3::IfcHalfSpaceSolid>(l),halfspace) ) return false;	
	TopoDS_Wire wire;
	if ( ! IfcGeom::convert_wire(l->PolygonalBoundary(),wire) || ! wire.Closed() ) return false;	
	gp_Trsf trsf;
	convert(l->Position(),trsf);
	TopoDS_Shape prism = BRepPrimAPI_MakePrism(BRepBuilderAPI_MakeFace(wire),gp_Vec(0,0,200));
	gp_Trsf down; down.SetTranslation(gp_Vec(0,0,-100.0));
	prism.Move(trsf*down);
	shape = BRepAlgoAPI_Common(halfspace,prism);
	return true;
}
bool IfcGeom::convert(const Ifc2x3::IfcShellBasedSurfaceModel::ptr l, IfcRepresentationShapeItems& shapes) {
	IfcUtil::IfcAbstractSelect::list shells = l->SbsmBoundary();
	const SurfaceStyle* collective_style = get_style(l);
	for( IfcUtil::IfcAbstractSelect::it it = shells->begin(); it != shells->end(); ++ it ) {
		TopoDS_Shape s;
		const SurfaceStyle* shell_style = 0;
		if ((*it)->is(Ifc2x3::Type::IfcRepresentationItem)) {
			shell_style = get_style((Ifc2x3::IfcRepresentationItem*)*it);
		}
		if (IfcGeom::convert_shape(*it,s)) {
			shapes.push_back(IfcRepresentationShapeItem(s, shell_style ? shell_style : collective_style));
		}
	}
	return true;
}
bool IfcGeom::convert(const Ifc2x3::IfcBooleanClippingResult::ptr l, TopoDS_Shape& shape) {
	TopoDS_Shape s1, s2;
	TopoDS_Wire boundary_wire;
	Ifc2x3::IfcBooleanOperand operand1 = l->FirstOperand();
	Ifc2x3::IfcBooleanOperand operand2 = l->SecondOperand();
	bool is_halfspace = operand2->is(Ifc2x3::Type::IfcHalfSpaceSolid);

	if ( ! IfcGeom::convert_shape(operand1,s1) )
		return false;

	const double first_operand_volume = shape_volume(s1);
	if ( first_operand_volume <= ALMOST_ZERO )
		Logger::Message(Logger::LOG_WARNING,"Empty solid for:",l->FirstOperand()->entity);

	if ( !IfcGeom::convert_shape(l->SecondOperand(),s2) ) {
		shape = s1;
		Logger::Message(Logger::LOG_ERROR,"Failed to convert SecondOperand of:",l->entity);
		return true;
	}

	if ( ! is_halfspace ) {
		const double second_operand_volume = shape_volume(s2);
		if ( second_operand_volume <= ALMOST_ZERO )
			Logger::Message(Logger::LOG_WARNING,"Empty solid for:",operand2->entity);
	}

	bool valid_cut = false;
	BRepAlgoAPI_Cut brep_cut(s1,s2);
	if ( brep_cut.IsDone() ) {
		TopoDS_Shape result = brep_cut;

		ShapeFix_Shape fix(result);
		fix.Perform();
		result = fix.Shape();
		
		bool is_valid = BRepCheck_Analyzer(result).IsValid() != 0;
		if ( is_valid ) {
			shape = result;
			valid_cut = true;
		} 
	}

	if ( valid_cut ) {
		const double volume_after_subtraction = shape_volume(shape);
		if ( ALMOST_THE_SAME(first_operand_volume,volume_after_subtraction) )
			Logger::Message(Logger::LOG_WARNING,"Subtraction yields unchanged volume:",l->entity);
	} else {
		Logger::Message(Logger::LOG_ERROR,"Failed to process subtraction:",l->entity);
		shape = s1;
	}

	return true;
}
bool IfcGeom::convert(const Ifc2x3::IfcConnectedFaceSet::ptr l, TopoDS_Shape& shape) {
	Ifc2x3::IfcFace::list faces = l->CfsFaces();
	bool facesAdded = false;
	const unsigned int num_faces = faces->Size();
	bool valid_shell = false;
	if ( num_faces < GetValue(GV_MAX_FACES_TO_SEW) ) {
		BRepOffsetAPI_Sewing builder;
		builder.SetTolerance(GetValue(GV_POINT_EQUALITY_TOLERANCE));
		builder.SetMaxTolerance(GetValue(GV_POINT_EQUALITY_TOLERANCE));
		builder.SetMinTolerance(GetValue(GV_POINT_EQUALITY_TOLERANCE));
		for( Ifc2x3::IfcFace::it it = faces->begin(); it != faces->end(); ++ it ) {
			TopoDS_Face face;
			bool converted_face = false;
			try {
				converted_face = IfcGeom::convert_face(*it,face);
			} catch (...) {}
			if ( converted_face && face_area(face) > GetValue(GV_MINIMAL_FACE_AREA) ) {
				builder.Add(face);
				facesAdded = true;
			} else {
				Logger::Message(Logger::LOG_WARNING,"Invalid face:",(*it)->entity);
			}
		}
		if ( ! facesAdded ) return false;
		try {
			builder.Perform();
			shape = builder.SewedShape();
			valid_shell = BRepCheck_Analyzer(shape).IsValid();
		} catch(...) {}
		if (valid_shell) {
			try {
				ShapeFix_Solid solid;
				solid.LimitTolerance(GetValue(GV_POINT_EQUALITY_TOLERANCE));
				TopoDS_Solid solid_shape = solid.SolidFromShell(TopoDS::Shell(shape));
				if (!solid_shape.IsNull()) {
					try {
						BRepClass3d_SolidClassifier classifier(solid_shape);
						shape = solid_shape;
					} catch (...) {}
				}
			} catch(...) {}
		} else {
			Logger::Message(Logger::LOG_WARNING,"Failed to sew faceset:",l->entity);
		}
	}
	if (!valid_shell) {
		TopoDS_Compound compound;
		BRep_Builder builder;
		builder.MakeCompound(compound);
		for( Ifc2x3::IfcFace::it it = faces->begin(); it != faces->end(); ++ it ) {
			TopoDS_Face face;
			bool converted_face = false;
			try {
				converted_face = IfcGeom::convert_face(*it,face);
			} catch (...) {}
			if ( converted_face && face_area(face) > GetValue(GV_MINIMAL_FACE_AREA) ) {
				builder.Add(compound,face);
				facesAdded = true;
			} else {
				Logger::Message(Logger::LOG_WARNING,"Invalid face:",(*it)->entity);
			}
		}
		if ( ! facesAdded ) return false;
		shape = compound;
	}
	return true;
}
bool IfcGeom::convert(const Ifc2x3::IfcMappedItem::ptr l, IfcRepresentationShapeItems& shapes) {
	gp_GTrsf gtrsf;
	Ifc2x3::IfcCartesianTransformationOperator::ptr transform = l->MappingTarget();
	if ( transform->is(Ifc2x3::Type::IfcCartesianTransformationOperator3DnonUniform) ) {
		IfcGeom::convert(reinterpret_pointer_cast<Ifc2x3::IfcCartesianTransformationOperator,
			Ifc2x3::IfcCartesianTransformationOperator3DnonUniform>(transform),gtrsf);
	} else if ( transform->is(Ifc2x3::Type::IfcCartesianTransformationOperator2DnonUniform) ) {
		Logger::Message(Logger::LOG_ERROR, "Unsupported MappingTarget:", transform->entity);
		return false;
	} else if ( transform->is(Ifc2x3::Type::IfcCartesianTransformationOperator3D) ) {
		gp_Trsf trsf;
		IfcGeom::convert(reinterpret_pointer_cast<Ifc2x3::IfcCartesianTransformationOperator,
			Ifc2x3::IfcCartesianTransformationOperator3D>(transform),trsf);
		gtrsf = trsf;
	} else if ( transform->is(Ifc2x3::Type::IfcCartesianTransformationOperator2D) ) {
		gp_Trsf2d trsf_2d;
		IfcGeom::convert(reinterpret_pointer_cast<Ifc2x3::IfcCartesianTransformationOperator,
			Ifc2x3::IfcCartesianTransformationOperator2D>(transform),trsf_2d);
		gtrsf = (gp_Trsf) trsf_2d;
	}
	Ifc2x3::IfcRepresentationMap::ptr map = l->MappingSource();
	Ifc2x3::IfcAxis2Placement placement = map->MappingOrigin();
	gp_Trsf trsf;
	if (placement->is(Ifc2x3::Type::IfcAxis2Placement3D)) {
		IfcGeom::convert((Ifc2x3::IfcAxis2Placement3D*)placement,trsf);
	} else {
		gp_Trsf2d trsf_2d;
		IfcGeom::convert((Ifc2x3::IfcAxis2Placement2D*)placement,trsf_2d);
		trsf = trsf_2d;
	}
	gtrsf.Multiply(trsf);
	const unsigned int previous_size = (const unsigned int) shapes.size();
	bool b = IfcGeom::convert_shapes(map->MappedRepresentation(),shapes);
	for ( unsigned int i = previous_size; i < shapes.size(); ++ i ) {
		shapes[i].append(gtrsf);
	}
	return b;
}

bool IfcGeom::convert(const Ifc2x3::IfcShapeRepresentation::ptr l, IfcRepresentationShapeItems& shapes) {
	Ifc2x3::IfcRepresentationItem::list items = l->Items();
	bool part_succes = false;
	if ( items->Size() ) {
		for ( Ifc2x3::IfcRepresentationItem::it it = items->begin(); it != items->end(); ++ it ) {
			Ifc2x3::IfcRepresentationItem* representation_item = *it;
			if ( IfcGeom::is_shape_collection(representation_item) ) {
				part_succes |= IfcGeom::convert_shapes(*it, shapes);
			} else {
				TopoDS_Shape s;
				if (IfcGeom::convert_shape(representation_item,s)) {
					shapes.push_back(IfcRepresentationShapeItem(s, get_style(representation_item)));
					part_succes |= true;
				}
			}
		}
	}
	return part_succes;
}