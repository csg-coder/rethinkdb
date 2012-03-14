#include <fstream>

#include "errors.hpp"
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>

#include "clustering/administration/persist.hpp"

namespace metadata_persistence {

std::string metadata_file(const std::string& file_path) {
    return file_path + "/metadata";
}

std::string errno_to_string(int err) {
    char buffer[200];
    char *res = strerror_r(err, buffer, sizeof(buffer));
    return std::string(res);
}

bool check_existence(const std::string& file_path) THROWS_ONLY(file_exc_t) {
    int res = access(file_path.c_str(), F_OK);
    if (res == 0) {
        return true;
    } else if (errno == ENOENT) {
        return false;
    } else {
        throw file_exc_t(errno_to_string(errno));
    }
}

void create(const std::string& file_path, machine_id_t machine_id, const cluster_semilattice_metadata_t &semilattice) THROWS_ONLY(file_exc_t) {

    int res = mkdir(file_path.c_str(), 0755);
    if (res != 0) {
        throw file_exc_t("Could not create directory: " + errno_to_string(errno));
    }

    std::ofstream file(metadata_file(file_path).c_str(), std::ios_base::trunc | std::ios_base::out);
    if (file.fail()) {
        throw file_exc_t("Could not write to file.");
    }

    boost::archive::text_oarchive archive(file);
    archive << machine_id;
    archive << semilattice;
}

void update(const std::string& file_path, machine_id_t machine_id, const cluster_semilattice_metadata_t &semilattice) THROWS_ONLY(file_exc_t) {
    std::ofstream file(metadata_file(file_path).c_str(), std::ios_base::trunc | std::ios_base::out);
    if (file.fail()) {
        throw file_exc_t("Could not write to file.");
    }

    boost::archive::text_oarchive archive(file);
    archive << machine_id;
    archive << semilattice;
}

void read(const std::string& file_path, machine_id_t *machine_id_out, cluster_semilattice_metadata_t *semilattice_out) THROWS_ONLY(file_exc_t) {

    std::ifstream file(metadata_file(file_path).c_str(), std::ios_base::in);
    if (file.fail()) {
        throw file_exc_t("Could not read from file.");
    }

    try {
        boost::archive::text_iarchive archive(file);
        archive >> *machine_id_out;
        archive >> *semilattice_out;
    } catch (boost::archive::archive_exception e) {
        throw file_exc_t(std::string("File contents invalid: ") + e.what());
    }
}

semilattice_watching_persister_t::semilattice_watching_persister_t(
        const std::string &fp,
        machine_id_t mi,
        boost::shared_ptr<semilattice_read_view_t<cluster_semilattice_metadata_t> > v,
        local_issue_tracker_t *lit) :
    file_path(fp), machine_id(mi), view(v),
    subs(boost::bind(&semilattice_watching_persister_t::dump, this), v),
    issue_tracker(lit)
{
    dump();
}

void semilattice_watching_persister_t::dump() {
    try {
        update(file_path, machine_id, view->get());
    } catch (file_exc_t e) {
        persistence_issue.reset(new local_issue_tracker_t::entry_t(
            issue_tracker,
            clone_ptr_t<local_issue_t>(new persistence_issue_t(e.what()))
            ));
        /* TODO: Should we set a timer to retry? */
        return;
    }
    persistence_issue.reset();
}

}  // namespace metadata_persistence
