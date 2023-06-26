// The Notices and Disclaimers for Ocean Worlds Autonomy Testbed for Exploration
// Research and Simulation can be found in README.md in the root directory of
// this repository.

#include <string>
#include <stdexcept>

#include <MaterialDistributionPlugin.h>

#include <populate_materials.h>

using std::string, std::make_unique, std::endl;
using ignition::math::Vector3d;

using namespace ow_materials;
using namespace gazebo;

const string PLUGIN_NAME = "MaterialDistributionPlugin";

const string NAMESPACE_MATERIALS = "/ow_materials";

const string PARAMETER_CORNER_A         = "corner_a";
const string PARAMETER_CORNER_B         = "corner_b";
const string PARAMETER_CELL_SIDE_LENGTH = "cell_side_length";

void MaterialDistributionPlugin::Load(physics::ModelPtr model,
                                      sdf::ElementPtr sdf)
{
  m_model = model;

  if (!sdf->HasElement(PARAMETER_CORNER_A)
      || !sdf->HasElement(PARAMETER_CORNER_B)) {
    gzerr << "Both " << PARAMETER_CORNER_A << " and " << PARAMETER_CORNER_B
          << " parameters are required." << endl;
    return;
  }
  if (!sdf->HasElement(PARAMETER_CELL_SIDE_LENGTH)) {
    gzerr << PARAMETER_CELL_SIDE_LENGTH << " is required." << endl;
    return;
  }

  const auto corner_a = sdf->Get<Vector3d>(PARAMETER_CORNER_A);
  const auto corner_b = sdf->Get<Vector3d>(PARAMETER_CORNER_B);
  const auto cell_side_length = sdf->Get<double>(PARAMETER_CELL_SIDE_LENGTH);

  try {
    m_grid = make_unique<AxisAlignedGrid<MaterialBlend>>(
      corner_a.X(), corner_a.Y(), corner_a.Z(),
      corner_b.X(), corner_b.Y(), corner_b.Z(),
      cell_side_length, MaterialBlend()
    );
  } catch (const GridConfigError &e) {
    gzerr << e.what() << endl;
    return;
  }

  const auto center = m_grid->getCenter();
  const auto diagonal = m_grid->getDiagonal();
  gzlog << PLUGIN_NAME << ": Material grid centered at (" << center.X() << ", "
                                                          << center.Y() << ", "
                                                          << center.Z() << ") "
        << "with dimensions " << diagonal.X() << " x "
                              << diagonal.Y() << " x "
                              << diagonal.Z() << " meters. " << endl;

  m_material_db = make_unique<MaterialDatabase>();
  // populate materials database
  try {
    populate_material_database(m_material_db.get(), NAMESPACE_MATERIALS);
  } catch (const MaterialConfigError &e) {
    gzerr << e.what() << endl;
    return;
  }
  gzlog << PLUGIN_NAME << ": Materials database populated with "
        << m_material_db->size() << " materials." << endl;

  gzlog << PLUGIN_NAME << ": Successfully loaded!" << endl;

}
