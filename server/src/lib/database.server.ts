import Database from "better-sqlite3";

const db = new Database("/app/data/data.db");

db.pragma("journal_mode = WAL");

db.exec(`
    CREATE TABLE IF NOT EXISTS controller
    (
        id INTEGER PRIMARY KEY NOT NULL,
        name TEXT NOT NULL UNIQUE
    );
        
    CREATE TABLE IF NOT EXISTS sensor
    (
        id INTEGER PRIMARY KEY NOT NULL,
        controller_id INTEGER NOT NULL,
        name TEXT NOT NULL,
        FOREIGN KEY (controller_id) REFERENCES controller(id),
        UNIQUE (controller_id, name)
    );
        
    CREATE TABLE IF NOT EXISTS sensor_reading (
        id INTEGER PRIMARY KEY NOT NULL,
        sensor_id INTEGER NOT NULL,
        name VARCHAR(64) NOT NULL,
        time_unix INT8 NOT NULL,
        value_str VARCHAR(255),
        value_int INTEGER,
        value_real REAL,
        FOREIGN KEY (sensor_id) REFERENCES sensor(id)
    );
`)

const addControllerQuery = db.prepare("INSERT INTO controller (name) VALUES (@name) ON CONFLICT DO NOTHING;");
const addSensorQuery = db.prepare(`
    INSERT INTO sensor (controller_id, name) 
    VALUES ((SELECT id FROM controller WHERE name = @controllerName), @sensorName) 
    ON CONFLICT DO NOTHING;
`);
const addSensorReadingQuery = db.prepare(`
    INSERT INTO sensor_reading (sensor_id, name, time_unix, value_str, value_int, value_real) 
    VALUES (
        (SELECT s.id FROM sensor s 
         JOIN controller c ON s.controller_id = c.id 
         WHERE c.name = @controllerName AND s.name = @sensorName),
        @readingName, 
        @timeUnix,
        @valueStr, 
        @valueInt, 
        @valueReal
    )
`);

export function addController(name: string) {
    addControllerQuery.run({ name });
}

export function addSensor(controller: string, name: string) {
    addSensorQuery.run({
        controllerName: controller,
        sensorName: name
    });
}

export function addSensorReading(controller: string, sensor: string, ts: number, name: string, value: any) {
    let valueStr: string | null = null;
    let valueInt: number | null = null;
    let valueReal: number | null = null;

    // Check if string represents a number
    const trimmed = value.toString().trim();

    // Try to parse as integer first
    if (/^-?\d+$/.test(trimmed)) {
        const parsed = parseInt(trimmed, 10);
        if (!isNaN(parsed)) {
            valueInt = parsed;
        } else {
            valueStr = value;
        }
    }
    // Try to parse as float
    else if (/^-?\d*\.?\d+([eE][+-]?\d+)?$/.test(trimmed)) {
        const parsed = parseFloat(trimmed);
        if (!isNaN(parsed)) {
            valueReal = parsed;
        } else {
            valueStr = value;
        }
    }
    // Otherwise store as string
    else {
        valueStr = value;
    }

    addSensorReadingQuery.run({
        controllerName: controller,
        sensorName: sensor,
        readingName: name,
        timeUnix: ts, // Current Unix timestamp
        valueStr,
        valueInt,
        valueReal
    });
}

// Query statements
const getAllControllersQuery = db.prepare("SELECT id, name FROM controller ORDER BY name");

const getSensorsByControllerQuery = db.prepare(`
    SELECT s.id, s.name, s.controller_id 
    FROM sensor s 
    JOIN controller c ON s.controller_id = c.id 
    WHERE c.name = @controllerName 
    ORDER BY s.name
`);

const getSensorReadingsQuery = db.prepare(`
    SELECT 
        sr.id,
        sr.sensor_id,
        sr.name as reading_name,
        sr.value_str,
        sr.value_int,
        sr.value_real,
        sr.time_unix,
        s.name as sensor_name,
        c.name as controller_name
    FROM sensor_reading sr
    JOIN sensor s ON sr.sensor_id = s.id
    JOIN controller c ON s.controller_id = c.id
    WHERE c.name = @controllerName AND s.name = @sensorName
    ORDER BY sr.time_unix DESC
    LIMIT @limit OFFSET @offset
`);

const getSensorReadingsWithTimeRangeQuery = db.prepare(`
    SELECT 
        sr.id,
        sr.sensor_id,
        sr.name as reading_name,
        sr.value_str,
        sr.value_int,
        sr.value_real,
        sr.time_unix,
        s.name as sensor_name,
        c.name as controller_name
    FROM sensor_reading sr
    JOIN sensor s ON sr.sensor_id = s.id
    JOIN controller c ON s.controller_id = c.id
    WHERE c.name = @controllerName AND s.name = @sensorName 
    AND sr.time_unix >= @startUnix AND sr.time_unix <= @endUnix
    ORDER BY sr.time_unix DESC
    LIMIT @limit OFFSET @offset
`);

const countSensorReadingsQuery = db.prepare(`
    SELECT COUNT(*) as total
    FROM sensor_reading sr
    JOIN sensor s ON sr.sensor_id = s.id
    JOIN controller c ON s.controller_id = c.id
    WHERE c.name = @controllerName AND s.name = @sensorName
`);

const countSensorReadingsWithTimeRangeQuery = db.prepare(`
    SELECT COUNT(*) as total
    FROM sensor_reading sr
    JOIN sensor s ON sr.sensor_id = s.id
    JOIN controller c ON s.controller_id = c.id
    WHERE c.name = @controllerName AND s.name = @sensorName 
    AND sr.time_unix >= @startUnix AND sr.time_unix <= @endUnix
`);

const getControllerFirstReadingUnixQuery = db.prepare(`
    SELECT sr.time_unix
    FROM sensor_reading sr
    JOIN sensor s ON sr.sensor_id = s.id
    JOIN controller c ON s.controller_id = c.id
    WHERE c.name = @controllerName
    ORDER BY sr.time_unix ASC
    LIMIT 1;
`);
const getControllerLastReadingUnixQuery = db.prepare(`
    SELECT sr.time_unix
    FROM sensor_reading sr
    JOIN sensor s ON sr.sensor_id = s.id
    JOIN controller c ON s.controller_id = c.id
    WHERE c.name = @controllerName 
    ORDER BY sr.time_unix DESC
    LIMIT 1;
`);

// Types
interface Controller {
    id: number;
    name: string;
}

interface Sensor {
    id: number;
    name: string;
    controller_id: number;
}

interface SensorReading {
    id: number;
    sensor_id: number;
    reading_name: string;
    value_str: string | null;
    value_int: number | null;
    value_real: number | null;
    time_unix: number; // Unix timestamp
    sensor_name: string;
    controller_name: string;
    // Computed properties
    value: string | number | null;
    timestamp: Date; // Converted to Date object
}

interface PaginatedResult<T> {
    data: T[];
    pagination: {
        page: number;
        limit: number;
        total: number;
        totalPages: number;
        hasNext: boolean;
        hasPrev: boolean;
    };
}

// Helper function to convert various time inputs to Unix timestamp
function toUnixTimestamp(time: string | Date | number): number {
    if (typeof time === 'number') {
        return time;
    } else if (time instanceof Date) {
        return Math.floor(time.getTime() / 1000);
    } else {
        // Assume ISO string
        return Math.floor(new Date(time).getTime() / 1000);
    }
}

// Query functions
export function getAllControllers(): Controller[] {
    return getAllControllersQuery.all() as Controller[];
}

export function getSensorsByController(controllerName: string): Sensor[] {
    return getSensorsByControllerQuery.all({ controllerName }) as Sensor[];
}

export function getControllerFirstReadingUnix(controllerName: string): number {
    return getControllerFirstReadingUnixQuery.get({ controllerName }).time_unix as number;
}
export function getControllerLastReadingUnix(controllerName: string): number {
    return getControllerLastReadingUnixQuery.get({ controllerName }).time_unix as number;
}

const getSensorReadingKeyCountQuery = db.prepare(`
    SELECT COUNT (distinct sr.name) as total
    FROM sensor_reading sr
    JOIN sensor s ON sr.sensor_id = s.id
    JOIN controller c ON s.controller_id = c.id
    WHERE c.name = @controllerName 
    AND   s.name = @sensorName
    LIMIT 1;
`);

export function getSensorReadingKeyCount(controllerName: string, sensorName: string): number {
    return getSensorReadingKeyCountQuery.get({ controllerName, sensorName }).total as number;

}

export function getSensorReadings(
    controllerName: string,
    sensorName: string,
    page: number = 1,
    limit: number = 50
): PaginatedGroupedResult {
    const offset = (page - 1) * limit;

    // Get total count
    const countResult = countSensorReadingsQuery.get({
        controllerName,
        sensorName
    }) as { total: number };
    const total = countResult.total;

    // Get paginated data
    const rawReadings = getSensorReadingsQuery.all({
        controllerName,
        sensorName,
        limit,
        offset
    });

    // Process readings to add computed values
    const readings: SensorReading[] = rawReadings.map((reading: any) => ({
        ...reading,
        value: reading.value_int ?? reading.value_real ?? reading.value_str,
        timestamp: new Date(reading.time_unix * 1000) // Convert Unix timestamp to Date
    }));

    // Group readings by reading_name
    const groupedReadings: GroupedSensorReadings = {};
    readings.forEach(reading => {
        if (!groupedReadings[reading.reading_name]) {
            groupedReadings[reading.reading_name] = [];
        }
        groupedReadings[reading.reading_name].push(reading);
    });

    const totalPages = Math.ceil(total / limit);

    return {
        data: groupedReadings,
        pagination: {
            page,
            limit,
            total,
            totalPages,
            hasNext: page < totalPages,
            hasPrev: page > 1
        }
    };
}

interface GroupedSensorReadings {
    [reading_name: string]: SensorReading[];
}

interface PaginatedGroupedResult {
    data: GroupedSensorReadings;
    pagination: {
        page: number;
        limit: number;
        total: number;
        totalPages: number;
        hasNext: boolean;
        hasPrev: boolean;
    };
}

export function getSensorReadingsInTimeRange(
    controllerName: string,
    sensorName: string,
    startTime: string | Date | number,
    endTime: string | Date | number,
    page: number = 1,
    limit: number = 50
): PaginatedGroupedResult {
    const offset = (page - 1) * limit;

    // Convert to Unix timestamps
    const startUnix = toUnixTimestamp(startTime);
    const endUnix = toUnixTimestamp(endTime);

    // Get total count
    const countResult = countSensorReadingsWithTimeRangeQuery.get({
        controllerName,
        sensorName,
        startUnix,
        endUnix
    }) as { total: number };
    const total = countResult.total;

    // Get paginated data
    const rawReadings = getSensorReadingsWithTimeRangeQuery.all({
        controllerName,
        sensorName,
        startUnix,
        endUnix,
        limit,
        offset
    });

    // Process readings to add computed values
    const readings: SensorReading[] = rawReadings.map((reading: any) => ({
        ...reading,
        value: reading.value_int ?? reading.value_real ?? reading.value_str,
        timestamp: new Date(reading.time_unix * 1000) // Convert Unix timestamp to Date
    }));

    // Group readings by reading_name
    const groupedReadings: GroupedSensorReadings = {};
    readings.forEach(reading => {
        if (!groupedReadings[reading.reading_name]) {
            groupedReadings[reading.reading_name] = [];
        }
        groupedReadings[reading.reading_name].push(reading);
    });

    const totalPages = Math.ceil(total / limit);

    return {
        data: groupedReadings,
        pagination: {
            page,
            limit,
            total,
            totalPages,
            hasNext: page < totalPages,
            hasPrev: page > 1
        }
    };
}


export default db;