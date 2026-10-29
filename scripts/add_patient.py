# scripts/add_patient.py - CLI tool to register a new patient and IoT WBAN node
import sys
import argparse
from pathlib import Path

BASE_DIR = Path(__file__).resolve().parent
ROOT_DIR = BASE_DIR.parent
if str(ROOT_DIR) not in sys.path:
    sys.path.insert(0, str(ROOT_DIR))

from db.session import get_db, init_db
from db.models import Patient

def list_patients():
    with get_db() as db:
        patients = db.query(Patient).all()
        print("\n" + "=" * 80)
        print(f"{'PATIENT ID':<12} {'NAME':<20} {'AGE':<6} {'GENDER':<8} {'ROOM':<14} {'IP ADDRESS':<16} {'STATUS'}")
        print("=" * 80)
        for p in patients:
            print(f"{p.patient_id:<12} {p.name:<20} {p.age:<6} {p.gender:<8} {p.room:<14} {p.ip_address:<16} {p.status}")
        print("=" * 80 + f"\nTotal Patients Registered: {len(patients)}\n")

def add_patient(patient_id: str, name: str, age: int, gender: str, room: str, ip_address: str, mac_address: str = None):
    patient_id = patient_id.strip().upper()
    with get_db() as db:
        existing = db.query(Patient).filter_by(patient_id=patient_id).first()
        if existing:
            print(f"[ERROR] Patient with ID '{patient_id}' already exists in the database ({existing.name}, Room {existing.room}).")
            sys.exit(1)

        new_p = Patient(
            patient_id=patient_id,
            name=name.strip(),
            age=int(age),
            gender=gender.strip(),
            room=room.strip(),
            ip_address=ip_address.strip(),
            mac_address=mac_address.strip() if mac_address else None,
            status="NORMAL"
        )
        db.add(new_p)
        db.commit()
        print(f"\n[SUCCESS] Patient '{patient_id}' ({name}) registered successfully in SQLite database!")
        print(f"  - Room: {room}")
        print(f"  - Allocated IP: {ip_address}")
        print(f"  - MQTT Vitals Topic: wban/{patient_id}/vitals")
        print(f"  - MQTT ECG Topic:    wban/{patient_id}/ecg")
        print(f"  - SDN QoS Priority Rule Target: IP {ip_address}:1883 -> Queue 1\n")

def main():
    parser = argparse.ArgumentParser(description="Register a new patient & IoT sensor node into the Healthcare SDN Database")
    parser.add_argument("--list", action="store_true", help="List all currently registered patients")
    parser.add_argument("--id", type=str, help="Unique Patient ID (e.g. P104)")
    parser.add_argument("--name", type=str, help="Patient full name (e.g. 'David Miller')")
    parser.add_argument("--age", type=int, default=50, help="Patient age (default: 50)")
    parser.add_argument("--gender", type=str, default="Male", choices=["Male", "Female", "Other"], help="Patient gender")
    parser.add_argument("--room", type=str, default="ICU Ward 304", help="Hospital room / bed number")
    parser.add_argument("--ip", type=str, help="Static or DHCP IP address assigned to the patient's IoT node (e.g. 10.0.0.4)")
    parser.add_argument("--mac", type=str, default=None, help="Optional MAC address of the ESP32 / sensor node")

    args = parser.parse_args()
    init_db()

    if args.list or len(sys.argv) == 1:
        list_patients()
        if len(sys.argv) == 1:
            print("Tip: To add a patient, run:")
            print('  python scripts/add_patient.py --id P104 --name "David Miller" --age 54 --gender Male --room "ICU Bed 4" --ip 10.0.0.4')
        return

    if not args.id or not args.name or not args.ip:
        print("[ERROR] --id, --name, and --ip are required arguments to register a new patient.")
        sys.exit(1)

    add_patient(
        patient_id=args.id,
        name=args.name,
        age=args.age,
        gender=args.gender,
        room=args.room,
        ip_address=args.ip,
        mac_address=args.mac
    )

if __name__ == "__main__":
    main()

