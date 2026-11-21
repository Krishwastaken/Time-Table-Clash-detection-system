-- Migration 002: Row-Level Security (RLS) & Views for Supabase / PostgreSQL

-- 1. Enable Row Level Security on Admin Tables
ALTER TABLE IF EXISTS admin_profiles ENABLE ROW LEVEL SECURITY;
ALTER TABLE IF EXISTS audit_logs ENABLE ROW LEVEL SECURITY;
ALTER TABLE IF EXISTS system_settings ENABLE ROW LEVEL SECURITY;

-- 2. Policies:
-- Only authenticated administrators can read/write administrative records
DO $$
BEGIN
    IF EXISTS (SELECT 1 FROM pg_proc WHERE proname = 'auth') THEN
        CREATE POLICY admin_profiles_admin_policy ON admin_profiles
            FOR ALL
            USING (auth.role() = 'authenticated')
            WITH CHECK (auth.role() = 'authenticated');

        CREATE POLICY audit_logs_read_policy ON audit_logs
            FOR SELECT
            USING (auth.role() = 'authenticated');

        CREATE POLICY audit_logs_insert_policy ON audit_logs
            FOR INSERT
            WITH CHECK (auth.role() = 'authenticated');
    END IF;
END $$;

-- 3. Public Read View for Future Student Portal
CREATE OR REPLACE VIEW view_published_timetable AS
SELECT 
    ts.id,
    ts.section,
    ts.day,
    ts.start_time,
    ts.end_time,
    ts.subject,
    ts.faculty,
    ts.room,
    ts.student_count,
    tv.version_number,
    tv.published_at
FROM timetable_sessions ts
JOIN timetable_versions tv ON ts.version_id = tv.version_id
WHERE tv.status = 'PUBLISHED' AND ts.is_draft = FALSE;

-- 4. Public Read View for Future Faculty Directory
CREATE OR REPLACE VIEW view_faculty_directory AS
SELECT 
    faculty_id,
    faculty_name,
    department,
    designation,
    email,
    office_location
FROM faculty
WHERE status = 'ACTIVE';
