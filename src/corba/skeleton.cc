#include "skeleton.hh"
#include "orb.hh"

namespace CORBA {

Skeleton::Skeleton(std::shared_ptr<CORBA::ORB> orb) : orb(orb), objectKey(orb->registerServant(this)) {}
Skeleton::Skeleton(std::shared_ptr<CORBA::ORB> orb, const std::string &objectKey) : orb(orb), objectKey(objectKey) { orb->registerServant(this, objectKey); }

Skeleton::~Skeleton() {}

};  // namespace CORBA
