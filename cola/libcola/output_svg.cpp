#include <string>
#include <iostream>
#include <sstream>
#include "output_svg.h"
#include "cola.h"
#include "straightener.h"
#include <list>

using namespace cola;
using vpsc::Rectangle;
using std::endl;
using std::cout;
using std::ios;
using std::max;
using std::min;
using std::ofstream;
using std::vector;
using std::list;

void OutputFile::generate() {
	unsigned E=es.size();
    bool cleanupRoutes=false;
    if(routes==NULL) {
        cleanupRoutes=true;
        routes = new vector<straightener::Edge*>(E);
        for(unsigned i=0;i<E;i++) {
            straightener::Route* r=new straightener::Route(2);
            r->xs[0]=rs[es[i].first]->getCentreX();
            r->ys[0]=rs[es[i].first]->getCentreY();
            r->xs[1]=rs[es[i].second]->getCentreX();
            r->ys[1]=rs[es[i].second]->getCentreY();
            (*routes)[i]=new straightener::Edge(i,es[i].first,es[i].second,r);
        }
    }
#if defined (CAIRO_HAS_SVG_SURFACE) && defined (CAIRO_HAS_PDF_SURFACE)

    double width,height,r=2;
	if(rects) r=rs[0]->width()/2;
	double xmin=DBL_MAX, ymin=xmin;
	double xmax=-DBL_MAX, ymax=xmax;
	for (unsigned i=0;i<rs.size();i++) {
		double x=rs[i]->getCentreX(), y=rs[i]->getCentreY();
		xmin=min(xmin,x);
		ymin=min(ymin,y);
		xmax=max(xmax,x);
		ymax=max(ymax,y);
	}
	xmax+=2*r;
	ymax+=2*r;
	xmin-=2*r;
	ymin-=2*r;
	width=xmax-xmin;
	height=ymax-ymin;

    Cairo::RefPtr<Cairo::Context> cr;
    openCairo(cr,width,height);

    /* set background colour
    cr->save(); // save the state of the context
    cr->set_source_rgb(0.86, 0.85, 0.47);
    cr->paint();    // fill image with the color
    cr->restore();  // color is back to black now
    */

    cr->set_line_width(1.);
    cr->set_font_size(8);
    cr->save();
    if(rc) for(Clusters::const_iterator c=rc->clusters.begin();c!=rc->clusters.end();c++) {
        draw_cluster_boundary(cr,**c,xmin,ymin);
    }
    if(curvedEdges)
        draw_curved_edges(cr,*routes,xmin,ymin);
    else 
        draw_edges(cr,*routes,xmin,ymin);
    Cairo::TextExtents te;
	for (unsigned i=0;i<rs.size();i++) {
		if(!rects) {
            double x=rs[i]->getCentreX()-xmin, y=rs[i]->getCentreY()-ymin;
            cr->arc(x,y,r, 0.0, 2.0 * M_PI);
            cr->fill();
		} else {
            double x=rs[i]->getMinX()-xmin, y=rs[i]->getMinY()-ymin;
            std::string str;
            if(labels) {
                str=std::string((*labels)[i]);
            } else {
                std::stringstream s; s<<i;
                str=s.str();
            }
            cr->get_text_extents(str,te);
            /*
            double llx = x-te.width/2.-1;
            double lly = y-te.height/2.-1;
            cr->rectangle(llx,lly,te.width+2,te.height+2);
            */
            cr->rectangle(x,y,
                    rs[i]->width(),rs[i]->height());
            cr->stroke_preserve();
            cr->save();
            cr->set_source_rgb(245./255., 233./255., 177./255.);
            cr->fill();
            cr->restore();
            if(labels) {
                cr->move_to(x-te.x_bearing-te.width/2.,y+te.height/2.);
                cr->show_text(str);
            }
            cr->stroke();
		}
	}

    cr->show_page();

    std::cout << "Wrote file \"" << fname << "\"" << std::endl;

#else

    std::cout << "You must compile cairo with SVG support for this to work."
        << std::endl;
#endif

    if(cleanupRoutes) {
        for(unsigned i=0;i<E;i++) {
            delete (*routes)[i];
        }
        delete routes;
    }
}

void OutputFile::draw_cluster_boundary(Cairo::RefPtr<Cairo::Context> const &cr,
        Cluster &c,
        const double xmin,
        const double ymin) {
    c.computeBoundary(rs);
    cr->save();
    // background
    cr->set_source_rgb(0.7, 0.7, 224./255.);
    cr->move_to(c.hullX[0]-xmin,c.hullY[0]-ymin);
    for(unsigned i=1;i<c.hullX.size();i++) {
        cr->line_to(c.hullX[i]-xmin,c.hullY[i]-ymin);
    }
    cr->line_to(c.hullX[0]-xmin,c.hullY[0]-ymin);
    cr->fill();
    cr->restore();
    // outline
    cr->move_to(c.hullX[0]-xmin,c.hullY[0]-ymin);
    for(unsigned i=1;i<c.hullX.size();i++) {
        cr->line_to(c.hullX[i]-xmin,c.hullY[i]-ymin);
    }
    cr->line_to(c.hullX[0]-xmin,c.hullY[0]-ymin);
    cr->stroke();
}

void OutputFile::draw_edges(Cairo::RefPtr<Cairo::Context> &cr,
 vector<straightener::Edge*> const & es, double const xmin, double const ymin) {
    cr->save();
    // background
    cr->set_source_rgba(0,0,1,0.5);
    for (unsigned i=0;i<es.size();i++) {
        cr->move_to(es[i]->route->xs[0]-xmin,es[i]->route->ys[0]-ymin);
		for (unsigned j=1;j<es[i]->route->n;j++) {
            cr->line_to(es[i]->route->xs[j]-xmin,es[i]->route->ys[j]-ymin);
		}
        cr->stroke();
	}
    cr->restore();
}

namespace bundles {
struct CEdge {
    unsigned startID, endID;
    double x0,y0,x1,y1,x2,y2,x3,y3;
};
struct CBundle;
struct CNode {
    double x,y;
    vector<CEdge*> edges;
    list<CBundle*> bundles;
};
double vangle(double ax,double ay, double bx, double by) {
    double ab=ax*bx+ay*by;
    double len=sqrt(ax*ax+ay*ay)*sqrt(bx*bx+by*by);
    double angle=acos(ab/len);
    //printf("ab=%f len=%f angle=%f\n",ab,len,angle);
    return angle;
}
struct CBundle {
    unsigned w;
    double x0, y0;
    double sx,sy;
    vector<CEdge*> edges;
    CBundle(CNode const &u) : w(u.edges.size()), x0(u.x), y0(u.y), sx(w*u.x), sy(w*u.y) { }
    void addEdge(CEdge *e) {
        if(x0==e->x0 && y0==e->y0) {
            sx+=e->x3; sy+=e->y3;
        } else {
            sx+=e->x0; sy+=e->y0;
        }
        edges.push_back(e);
    }
    double x1() const {
       return sx/(w+edges.size());
    } 
    double y1() const {
       return sy/(w+edges.size());
    } 
    double angle(CBundle* const &b) const {
        double ax=x1()-x0;
        double ay=y1()-y0;
        double bx=b->x1()-b->x0;
        double by=b->y1()-b->y0;
        return vangle(ax,ay,bx,by);
    }
    double yangle() const {
        double x=x1()-x0;
        double y=y1()-y0;
        double o=x<0?1:-1;
        return vangle(0,1,x,y)*o+M_PI;
    }
    void merge(CBundle* b) {
        for(unsigned i=0;i<b->edges.size();i++) {
            addEdge(b->edges[i]);
        }
    }
    void dump() {
        printf("Bundle: ");
        for(unsigned i=0;i<edges.size();i++) {
            printf("(%d,%d) ",edges[i]->startID,edges[i]->endID);
        }
    }
};
struct clockwise {
    bool operator() (CBundle* const &a, CBundle* const &b) {
        return a->yangle()<b->yangle();
    }
};
} //namespace bundles

/*
 * draw edges bundled.  That is, edges are drawn as splines, with the control points
 * between adjacent edges outgoing from a particular node shared if the angle between them
 * is less than pi/8
 */
void OutputFile::draw_curved_edges(Cairo::RefPtr<Cairo::Context> &cr,
        vector<straightener::Edge*> const & es, 
        const double xmin, 
        const double ymin) {
    using namespace bundles;
    vector<CNode> nodes(rs.size());
    vector<CEdge> edges(es.size());
    for (unsigned i=0;i<es.size();i++) {
        CEdge *e=&edges[i];
        unsigned start=es[i]->startNode;
        unsigned end=es[i]->endNode;
        e->startID=start;
        e->endID=end;
        nodes[start].x=rs[start]->getCentreX()-xmin;
        nodes[start].y=rs[start]->getCentreY()-ymin;
        nodes[end].x=rs[end]->getCentreX()-xmin;
        nodes[end].y=rs[end]->getCentreY()-ymin;
        e->x0=nodes[start].x;
        e->x1=nodes[start].x;
        e->x2=nodes[end].x;
        e->x3=nodes[end].x;
        e->y0=nodes[start].y;
        e->y1=nodes[start].y;
        e->y2=nodes[end].y;
        e->y3=nodes[end].y;
        nodes[end].edges.push_back(e);
        nodes[start].edges.push_back(e);
    }

    for (unsigned i=0;i<nodes.size();i++) {
        CNode u=nodes[i];
        if(u.edges.size()<2) continue;
        for (unsigned j=0;j<u.edges.size();j++) {
            CBundle* b=new CBundle(u);
            b->addEdge(u.edges[j]);
            u.bundles.push_back(b);
        }
        u.bundles.sort(clockwise());
        /*
        printf("Sorted:  \n");
        list<CBundle*>::iterator i,j;
        for(list<CBundle*>::iterator i=u.bundles.begin();i!=u.bundles.end();i++) {
                CBundle* a=*i;
                a->dump();
                printf("  angle=%f\n",a->yangle());
        }
        printf("---------\n");
        */
        while(true) {
            double minAngle=DBL_MAX;
            list<CBundle*>::iterator mini,minj,i,j;
            for(i=u.bundles.begin();i!=u.bundles.end();i++) {
                j=i;
                if(++j==u.bundles.end()) {
                    j=u.bundles.begin(); 
                }
                CBundle* a=*i;
                CBundle* b=*j;
                double angle=b->yangle()-a->yangle();
                if(angle<0) angle+=2*M_PI;
                //printf("between ");
                //a->dump(); b->dump();
                //printf(" angle=%f\n",angle);
                if(angle<minAngle) {
                    minAngle=angle;
                    mini=i;
                    minj=j;
                }
            }
            if(minAngle>cos(M_PI/8.)) break;
            CBundle* a=*mini;
            CBundle* b=*minj;
            //a->dump();
            //b->dump();
            b->merge(a);
            //printf("***Merged on %f***: ",minAngle);
            //b->dump();
            //printf("\n");
            u.bundles.erase(mini);
            if(u.bundles.size() < 2) break;
        }
        for(list<CBundle*>::iterator i=u.bundles.begin();i!=u.bundles.end();i++) {
            CBundle* b=*i;
            for(unsigned i=0;i<b->edges.size();i++) {
                CEdge* e=b->edges[i];
                if(e->x0==u.x&&e->y0==u.y) {
                    e->x1=b->x1();
                    e->y1=b->y1();
                } else {
                    e->x2=b->x1();
                    e->y2=b->y1();
                }
            }
        }
    }

    cr->save();
    // background
    cr->set_source_rgba(0,0,1,0.2);
    for (unsigned i=0;i<edges.size();i++) {
        CEdge &e=edges[i];
        cr->move_to(e.x0,e.y0);
        cr->curve_to(e.x1,e.y1,e.x2,e.y2,e.x3,e.y3);
        cr->stroke();
	}
    cr->restore();
}
void OutputFile::openCairo(Cairo::RefPtr<Cairo::Context> &cr, double width, double height) {
    if(fname.rfind("pdf") == (fname.length()-3) ) {
        printf("writing pdf file: %s\n",fname.c_str());
        Cairo::RefPtr<Cairo::PdfSurface> pdfsurface =
            Cairo::PdfSurface::create(fname, width, height);
        cr = Cairo::Context::create(pdfsurface);
    } else {
        printf("writing svg file: %s\n",fname.c_str());
        Cairo::RefPtr<Cairo::SvgSurface> svgsurface =
            Cairo::SvgSurface::create(fname, width, height);
        cr = Cairo::Context::create(svgsurface);
    }
}


// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=4:softtabstop=4:encoding=utf-8:textwidth=99 :
