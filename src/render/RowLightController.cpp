#include "RowLightController.h"
#include "MotionFunctions.h"
#include <algorithm>
#include <numbers>
namespace yalahoo {
double RowLightController::follow(ScalarSpring &s, double target, double dt, double duration) {
    const double previous=s.value, distance=target-previous;
    if (distance==0) {s.velocity=0; return previous;}
    const double rate=4.743864518390578/duration, direction=distance>0?1:-1;
    const double velocity=std::min(std::max(0.0,s.velocity*direction),rate*std::abs(distance))*direction;
    const double elapsed=std::max(0.0,dt), impulse=velocity-rate*distance, decay=std::exp(-rate*elapsed);
    const double next=target+(-distance+impulse*elapsed)*decay;
    s.value=std::clamp(next,std::min(previous,target),std::max(previous,target));
    s.velocity=s.value==target||s.value!=next?0:(velocity-rate*impulse*elapsed)*decay;
    return s.value;
}
double RowLightController::introOffset(double t) {
    return t<24.15?-9+27*smooth((t-22.2)/1.95):18*(1-smooth((t-24.15)/1.6));
}
void RowLightController::reset() {
    initialized=false; opening=false; basis.reset(); basisLane=0; independent=false;
    row.velocity=lane.velocity=0;
}
void RowLightController::update(const RowLightingInput &i,double dt) {
    if (!i.visible) {reset(); return;}
    const double nextBasis=i.origin.z-i.row*.62;
    const bool intro=!i.reduced&&i.introTime.has_value();
    if (!initialized||i.reduced||i.cinematic||opening) {row.finish(i.row);lane.finish(i.lane);}
    else {
        if (basis&&(i.independentColumns!=independent||(i.independentColumns&&i.lane!=basisLane)))
            row.value+=(*basis-nextBasis)/.62;
        follow(row,i.row,dt,settings.transitionDuration);follow(lane,i.lane,dt,settings.transitionDuration);
    }
    if (intro) row.value=i.row+introOffset(*i.introTime);
    opening=intro; initialized=true; basis=nextBasis; basisLane=i.lane; independent=i.independentColumns;
    column=i.origin;column.z+=(row.value-i.row)*.62;column.x+=(lane.value-i.lane)*5.2;
}
double RowLightController::sample(SceneVector origin) const {
    const double r=(origin.z-column.z-settings.offset*.62)/(.5*settings.width*.62), l=(origin.x-column.x)/27;
    return std::exp(-std::numbers::ln2*r*r)*std::exp(-std::pow(l,6));
}
}
