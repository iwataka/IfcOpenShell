package org.ifcopenshell.jni;

import java.io.File;

import org.ifcopenshell.model.IfcGeometryObject;
import org.ifcopenshell.model.IfcObject;

public class Library {

	public Library(String libPath) throws Exception{
		File file = new File(libPath);
		String absPath = file.getAbsolutePath();
		if (file.exists()) {
			System.load(absPath);
		}else{
			throw new Exception("Path not Found: " + absPath);
		}
	}

	public native boolean setIfcData(byte[] data);

	public native boolean setIfcPath(String fname);

	public native IfcGeometryObject getIfcGeometry();

	public native IfcObject getObject(int id);

	public native void Cleanup();
}
