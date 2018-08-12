/*
 * polysplit.cpp -- split polygons into constituents using OGR and GEOS
 *
 * written by Schuyler Erle <schuyler@nocat.net>
 * copyright (c) 2012 Geoloqi, Inc.
 * published under the 3-clause BSD license -- see README.txt for details.
 * 
 */

#include <cstdlib>
#include <iostream>
#include <vector>
#include <ogrsf_frmts.h>

#define MAXVERTICES 250
#define OUTPUTDRIVER "ESRI Shapefile"
#define OUTPUTTYPE wkbPolygon
#define IDFIELD "id"

#if GDAL_VERSION_MAJOR >= 2
    using gdal_dataset_type = GDALDataset;
#else
    using gdal_dataset_type = OGRDataSource;
#endif

typedef std::vector<OGRPolygon *> OGRPolyList;
typedef int feature_id_t;

static bool debug = false;

void split_polygons(OGRPolyList *pieces, OGRGeometry* geometry, int max_vertices) {
    /* split_polygons recursively splits the (multi)polygon into smaller
     * polygons until each polygon has at most max_vertices, and pushes each
     * one onto the pieces vector.
     * 
     * Multipolygons are automatically divided into their constituent polygons.
     * Empty polygons and other geometry types are ignored. Invalid polygons
     * get cleaned up to the best of our ability, but this does trigger
     * warnings from inside GEOS.
     *
     * Each polygon is split by dividing its bounding box into quadrants, and
     * then recursing on the intersection of each quadrant with the original
     * polygon, until the pieces are of the desired complexity.
     */

    if (geometry == NULL) {
        std::cerr << "WARNING: NULL geometry passed to split_polygons!\n";
        return;
    }
    
    if (geometry->IsEmpty())
        return;
    
    if (geometry->getGeometryType() == wkbMultiPolygon) {
        OGRMultiPolygon *multi = (OGRMultiPolygon*) geometry;
        for (int i = 0; i < multi->getNumGeometries(); i++) {
            split_polygons(pieces, multi->getGeometryRef(i), max_vertices);
        }
        return;
    } 
    
    if (geometry->getGeometryType() != wkbPolygon)
        return;
    
    OGRPolygon* polygon = (OGRPolygon*) geometry;
    if (polygon->getExteriorRing()->getNumPoints() <= max_vertices) {
        pieces->push_back((OGRPolygon*) polygon->clone());
        return;
    }

    bool polygonIsPwned = false;
    if (!polygon->IsValid() || !polygon->IsSimple()) {
        polygon = (OGRPolygon*) polygon->Buffer(0); // try to tidy it up
        polygonIsPwned = true; // now we own the reference and have to free it later
    }

    OGRPoint centroid;
    polygon->Centroid(&centroid);
    double cornerX = centroid.getX(),
           cornerY = centroid.getY();

    OGREnvelope envelope;
    polygon->getEnvelope(&envelope);

    for (int quadrant = 0; quadrant < 4; quadrant++) {
        OGREnvelope bbox(envelope);
        OGRLinearRing ring;
        OGRPolygon mask;
        switch (quadrant) { // in no particular order, actually
            case 0: bbox.MaxX = cornerX; bbox.MaxY = cornerY; break;
            case 1: bbox.MaxX = cornerX; bbox.MinY = cornerY; break;
            case 2: bbox.MinX = cornerX; bbox.MaxY = cornerY; break;
            case 3: bbox.MinX = cornerX; bbox.MinY = cornerY; break;
        }
        ring.setNumPoints(5);
        ring.setPoint(0, bbox.MinX, bbox.MinY);
        ring.setPoint(1, bbox.MinX, bbox.MaxY);
        ring.setPoint(2, bbox.MaxX, bbox.MaxY);
        ring.setPoint(3, bbox.MaxX, bbox.MinY);
        ring.setPoint(4, bbox.MinX, bbox.MinY); // close the ring
        mask.addRing(&ring);
        OGRGeometry* piece = mask.Intersection(polygon);
        split_polygons(pieces, piece, max_vertices);
        delete piece;
    } 

    if (polygonIsPwned) delete polygon;
}

gdal_dataset_type *create_destination(const char* drivername, const char* filename,
        const char *layername, const char *id_field_name) {

    /* Find the requested OGR output driver. */
#if GDAL_VERSION_MAJOR >= 2
    GDALDriver* driver = GetGDALDriverManager()->GetDriverByName(drivername);
#else
    OGRSFDriver* driver = OGRSFDriverRegistrar::GetRegistrar()->GetDriverByName(drivername);
#endif
    if( driver == NULL ) {
        std::cerr << drivername << " driver not available.\n";
        return NULL;
    }

    /* Create the output data source. */
#if GDAL_VERSION_MAJOR >= 2
    GDALDataset* ds = driver->Create( filename, 0, 0, 0, GDT_Unknown, 0);
#else
    OGRDataSource* ds = driver->CreateDataSource( filename, NULL );
#endif
    if( ds == NULL ) {
        std::cerr << "Creation of output file " << filename << " failed.\n";
        return NULL;
    }

    /* Create the output layer. */
    OGRLayer* layer;
    layer = ds->CreateLayer( layername, NULL, OUTPUTTYPE, NULL );
    if( layer == NULL ) {
        std::cerr << "Layer creation failed.\n";
        return NULL;
    }

    /* Add the ID field, which defaults to "id" if a name isn't given. */
    if (id_field_name == NULL) id_field_name = IDFIELD;
    OGRFieldDefn field( id_field_name, OFTInteger );
    if( layer->CreateField( &field ) != OGRERR_NONE ) {
        std::cerr <<  "Creating " << id_field_name << " field failed.\n";
        return NULL;
    }
    return ds;
}

void write_feature(OGRLayer *layer, OGRPolygon *geom, feature_id_t id) {
    /* Create a new feature from the geometry and ID, and write it to the
     * output layer. */
    OGRFeature *feature = OGRFeature::CreateFeature( layer->GetLayerDefn() );
    feature->SetField(0, id);
    feature->SetGeometryDirectly(geom); // saves having to destroy it manually
    if(layer->CreateFeature( feature ) != OGRERR_NONE) {
        std::cerr << "Failed to create feature in output.\n";
        exit( 1 );
    }
    OGRFeature::DestroyFeature( feature );
}

void usage(void) {
    std::cerr << "\nUsage: polysplit [opts] <input> <output>\n\n"
              << "\t-i\tinput layer name\n"
              << "\t-o\toutput layer name\n"
              << "\t-f\tOGR output driver name\n"
              << "\t-n\tID field name (must be integer type)\n"
              << "\t-m\tMax vertices per output polygon\n"
              << "\t-v\tVerbose mode\n\n";
    exit(1);
}

int main(int argc, char** argv) {
    const char *source_name, *src_layer_name = NULL,
               *dest_name, *dest_layer_name = NULL,
               *driver_name = OUTPUTDRIVER,
               *id_field_name = NULL;
    int max_vertices = MAXVERTICES,
        opt;

    while ((opt = getopt(argc, argv, "i:o:f:n:m:v")) != -1) {
        switch (opt) {
            case 'i': src_layer_name = optarg;      break;
            case 'o': dest_layer_name = optarg;     break;
            case 'f': driver_name = optarg;         break;
            case 'n': id_field_name = optarg;       break;
            case 'm': max_vertices = atoi(optarg);  break;
            case 'v': debug = true;                 break;
            default: usage();
        }
    }
    argc -= optind;
    argv += optind;

    if (argc < 2 || max_vertices <= 5) usage();
    source_name = argv[0];
    dest_name = argv[1];

    /* Register the OGR datasource drivers. */
#if GDAL_VERSION_MAJOR >= 2
    GDALAllRegister();
#else
    OGRRegisterAll();
#endif

    /* Open the input data source */
#if GDAL_VERSION_MAJOR >= 2
    GDALDataset* source = static_cast<GDALDataset *>(GDALOpen( source_name, GA_ReadOnly ));
#else
    OGRDataSource* source = OGRSFDriverRegistrar::Open( source_name, FALSE );
#endif
    if( source == NULL ) {
        std::cerr << "Opening " << source_name << " failed." << std::endl;
        exit( 1 );
    }

    /* Open the input layer. The first one is used if none is specified. */
    OGRLayer *srcLayer = (src_layer_name ? source->GetLayerByName(src_layer_name)
                                         : source->GetLayer(0));
    if( srcLayer == NULL ) {
        std::cerr << "Can't find input layer." << std::endl;
        exit( 1 );
    }

    /* Find the ID field on the input layer, if provided. Freak out if it's not
     * there, or if it's not an integer field. */
    int id_field = -1;
    if (id_field_name != NULL) {
        OGRFeatureDefn *layerDef = srcLayer->GetLayerDefn();
        id_field = layerDef->GetFieldIndex(id_field_name);
        if (id_field < 0) {
            std::cerr << "Can't find ID field " << id_field_name << "." << std::endl;
            exit( 1 );
        }
        OGRFieldDefn *fieldDef = layerDef->GetFieldDefn(id_field);
        if (fieldDef->GetType() != OFTInteger) {
            std::cerr << "ID field " << id_field_name << " isn't integer type!\n";
            exit( 1 );
        }
    } 
    
    /* Create the output data source. */
    gdal_dataset_type* dest = create_destination(driver_name, dest_name,
                                             dest_layer_name, id_field_name);
    if( dest == NULL ) exit( 1 );

    /* Get the output layer. */
    OGRLayer *destLayer = (dest_layer_name ? dest->GetLayerByName(dest_layer_name)
                                           : dest->GetLayer(0));

    /* Some stats. */
    int features_read = 0, features_written = 0,
        total = srcLayer->GetFeatureCount();

    /* Main loop: Iterate over each feature in the input layer. */
    OGRFeature *feature;
    srcLayer->ResetReading();

    while( (feature = srcLayer->GetNextFeature()) != NULL ) {
        /* Make an empty parts list, and get the ID and geometry from the
         * input. */
        OGRPolyList pieces;

        feature_id_t id = (id_field >= 0 ? feature->GetFieldAsInteger(id_field)
                                         : feature->GetFID());
        OGRGeometry *geometry = feature->GetGeometryRef();
        
        /* Recursively split the geometry, and write a new feature for each
         * polygon that comes out. */
        split_polygons(&pieces, geometry, max_vertices);
        for (OGRPolyList::iterator it = pieces.begin(); it != pieces.end(); it++) {
            write_feature(destLayer, *it, id);
            features_written++;
            /* We don't have to destroy each piece because write_feature calls
             * SetGeometryDirectly. */
        }

        features_read++;
        if (debug)
            std::cerr << features_read << " / " << total << "\r";

        OGRFeature::DestroyFeature( feature );
    }

    /* Close the input and output data sources. */
#if GDAL_VERSION_MAJOR >= 2
    GDALClose(source);
    GDALClose(dest);
#else
    OGRDataSource::DestroyDataSource( source );
    OGRDataSource::DestroyDataSource( dest );
#endif

    std::cerr << features_read << " features read, " 
              << features_written << " written.\n";
}
