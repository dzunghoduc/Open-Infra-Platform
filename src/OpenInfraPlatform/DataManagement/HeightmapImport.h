/*
    Copyright (c) 2018 Technical University of Munich
    Chair of Computational Modeling and Simulation.

    TUM Open Infra Platform is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License Version 3
    as published by the Free Software Foundation.

    TUM Open Infra Platform is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include "OpenInfraPlatform/Infrastructure/Import/Import.h"
#include "OpenInfraPlatform/Data/terrainDescription.h"

#include <BlueFramework/ImageProcessing/io.h>

#include <boost/noncopyable.hpp>

namespace OpenInfraPlatform
{
	namespace DataManagement
	{

		class BLUEINFRASTRUCTURE_API HeightmapImport : public buw::Import
		{
		public:
			HeightmapImport(const std::string& filename, const buw::Vector3d& offsetViewArea);

		};
	} // end namespace Infrastructure
} // end namespace BlueFramework

namespace buw
{
	using OpenInfraPlatform::DataManagement::HeightmapImport;
}

