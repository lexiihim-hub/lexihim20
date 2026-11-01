#include <iostream>
#include <string>
#include <vector>
#include <queue>
#include <stack>
#include <memory>
#include <algorithm>
#include <stdexcept>

namespace SCAPMS {

    // Custom Exception for Input Validation
    class ClinicException : public std::runtime_error {
    public:
        explicit ClinicException(const std::string& message) : std::runtime_error(message) {}
    };

    // Forward Declaration of Appointment
    class Appointment {
    public:
        std::string appointmentId;
        std::string date;
        std::string status; // "Pending", "Completed", "Cancelled"
        
        Appointment(std::string id, std::string dt, std::string stat = "Pending")
            : appointmentId(id), date(dt), status(stat) {}
    };

    // Abstract Interface for Appointable patient types
    class Appointable {
    public:
        virtual ~Appointable() = default;
        virtual void scheduleAppointment(const Appointment& app) = 0;
        virtual void cancelAppointment(const std::string& appId) = 0;
    };

    // Base Abstract Class for Patients
    class PatientRecord {
    protected:
        std::string patientId;
        std::string name;
        int age;
        std::string contactInfo;
        std::vector<Appointment> appointments;

    public:
        PatientRecord(std::string id, std::string nm, int ag, std::string contact)
            : patientId(id), name(nm), age(ag), contactInfo(contact) {
            if (ag < 0 || ag > 120) {
                throw ClinicException("Error: Patient age must be between 0 and 120.");
            }
        }
        virtual ~PatientRecord() = default;

        // Getters
        std::string getId() const { return patientId; }
        std::string getName() const { return name; }
        int getAge() const { return age; }
        const std::vector<Appointment>& getAppointments() const { return appointments; }
        std::vector<Appointment>& getModifiableAppointments() { return appointments; }

        // Pure virtual function enforcing runtime polymorphism
        virtual std::string getPatientCategory() const = 0;
        virtual void displayDetails() const = 0;
    };

    // Subclass 1: GeneralPatient
    class GeneralPatient : public PatientRecord, public Appointable {
    public:
        using PatientRecord::PatientRecord;

        std::string getPatientCategory() const override { return "General"; }
        
        void displayDetails() const override {
            std::cout << "[General] ID: " << patientId << " | Name: " << name << " | Age: " << age << "\n";
        }

        void scheduleAppointment(const Appointment& app) override {
            appointments.push_back(app);
        }

        void cancelAppointment(const std::string& appId) override {
            for (auto& app : appointments) {
                if (app.appointmentId == appId) {
                    app.status = "Cancelled";
                    return;
                }
            }
            throw ClinicException("Error: Appointment ID not found.");
        }
    };

    // Subclass 2: EmergencyPatient
    class EmergencyPatient : public PatientRecord {
    private:
        std::string triageLevel; // High Priority
    public:
        EmergencyPatient(std::string id, std::string nm, int ag, std::string contact, std::string triage = "High Priority")
            : PatientRecord(id, nm, ag, contact), triageLevel(triage) {}

        std::string getPatientCategory() const override { return "Emergency"; }

        void displayDetails() const override {
            std::cout << "[EMERGENCY - " << triageLevel << "] ID: " << patientId << " | Name: " << name << "\n";
        }
    };

    // Subclass 3: FollowUpPatient
    class FollowUpPatient : public PatientRecord, public Appointable {
    private:
        std::string previousDoctor;
    public:
        FollowUpPatient(std::string id, std::string nm, int ag, std::string contact, std::string doc)
            : PatientRecord(id, nm, ag, contact), previousDoctor(doc) {}

        std::string getPatientCategory() const override { return "Follow-Up"; }

        void displayDetails() const override {
            std::cout << "[Follow-Up] ID: " << patientId << " | Name: " << name << " | Previous Dr: " << previousDoctor << "\n";
        }

        void scheduleAppointment(const Appointment& app) override {
            appointments.push_back(app);
        }

        void cancelAppointment(const std::string& appId) override {
            for (auto& app : appointments) {
                if (app.appointmentId == appId) {
                    app.status = "Cancelled";
                    return;
                }
            }
        }
    };

    // Struct to capture Administrative Operations for the Undo feature
    struct AdminOperation {
        std::string operationType; // "DELETE", "ADD"
        std::shared_ptr<PatientRecord> patientData;
    };

    // Clinic Manager System Core Business Logic Component
    class ClinicManager {
    private:
        std::vector<std::shared_ptr<PatientRecord>> patients; // Core Database storage
        std::queue<std::shared_ptr<PatientRecord>> waitingQueue; // Consultation Queue
        std::stack<AdminOperation> undoStack; // Administrative Undo Engine
        std::shared_ptr<PatientRecord> recentCache[10]; // Fixed-size Cache Array
        int cacheSize = 0;

    public:
        // Core Feature: Register Patients
        void registerPatient(std::shared_ptr<PatientRecord> p) {
            for (const auto& existing : patients) {
                if (existing->getId() == p->getId()) {
                    throw ClinicException("Error: Patient ID already exists.");
                }
            }
            patients.push_back(p);
        }

        // Core Feature: Delete Patient with Undo Registration
        void deletePatient(const std::string& id) {
            for (auto it = patients.begin(); it != patients.end(); ++it) {
                if ((*it)->getId() == id) {
                    undoStack.push(AdminOperation{"DELETE", *it});
                    patients.erase(it);
                    return;
                }
            }
            throw ClinicException("Error: Patient records do not exist for ID.");
        }

        // Core Feature: Undo Feature Engine
        void undoLastOperation() {
            if (undoStack.empty()) {
                throw ClinicException("No administrative actions to undo.");
            }
            AdminOperation lastOp = undoStack.top();
            undoStack.pop();

            if (lastOp.operationType == "DELETE") {
                patients.push_back(lastOp.patientData);
                std::cout << "Undo successful: Restored patient ID " << lastOp.patientData->getId() << "\n";
            }
        }

        // Core Feature: Consultation Queue Management
        void addToQueue(const std::string& id) {
            for (auto& p : patients) {
                if (p->getId() == id) {
                    waitingQueue.push(p);
                    return;
                }
            }
            throw ClinicException("Patient not found to queue.");
        }

        void callNextPatient() {
            if (waitingQueue.empty()) {
                std::cout << "Waiting queue is empty.\n";
                return;
            }
            auto activePatient = waitingQueue.front();
            waitingQueue.pop();
            std::cout << "Now serving patient: " << activePatient->getName() << "\n";
            
            // Push to custom fixed-size array cache
            if (cacheSize < 10) {
                recentCache[cacheSize++] = activePatient;
            } else {
                for (int i = 1; i < 10; ++i) recentCache[i - 1] = recentCache[i];
                recentCache[9] = activePatient;
            }
        }

        // REQUIRED MANUAL ALGORITHM: Linear Search by Patient ID
        std::shared_ptr<PatientRecord> linearSearchById(const std::string& id) {
            for (const auto& p : patients) {
                if (p->getId() == id) return p;
            }
            return nullptr;
        }

        // REQUIRED MANUAL ALGORITHM: Binary Search by Age (Data must be pre-sorted by Age)
        std::shared_ptr<PatientRecord> binarySearchByAge(int targetAge) {
            int low = 0, high = static_cast<int>(patients.size()) - 1;
            while (low <= high) {
                int mid = low + (high - low) / 2;
                if (patients[mid]->getAge() == targetAge) return patients[mid];
                if (patients[mid]->getAge() < targetAge) low = mid + 1;
                else high = mid - 1;
            }
            return nullptr;
        }

        // REQUIRED MANUAL ALGORITHM: Recursive Search by Name
        int recursiveSearchNameHelper(int index, const std::string& targetName) {
            if (index >= static_cast<int>(patients.size())) return -1;
            if (patients[index]->getName() == targetName) return index;
            return recursiveSearchNameHelper(index + 1, targetName);
        }

        std::shared_ptr<PatientRecord> searchByName(const std::string& name) {
            int idx = recursiveSearchNameHelper(0, name);
            return (idx != -1) ? patients[idx] : nullptr;
        }

        // REQUIRED MANUAL ALGORITHM: Selection Sort by Patient Name
        void selectionSortByName() {
            int n = patients.size();
            for (int i = 0; i < n - 1; ++i) {
                int minIdx = i;
                for (int j = i + 1; j < n; ++j) {
                    if (patients[j]->getName() < patients[minIdx]->getName()) {
                        minIdx = j;
                    }
                }
                std::swap(patients[i], patients[minIdx]);
            }
        }

        // REQUIRED MANUAL ALGORITHM: Insertion Sort by Patient Age
        void insertionSortByAge() {
            int n = patients.size();
            for (int i = 1; i < n; ++i) {
                auto key = patients[i];
