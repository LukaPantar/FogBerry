
import * as db  from "$lib/database.server"

import { json, error } from '@sveltejs/kit';
import type { RequestHandler } from './$types';


export const POST: RequestHandler = async ({ request }) => {
    try {
        const body = await request.json();

        for (const entry of body.entries) {
            const controller = entry.controller;
            const timestamp = entry.timestamp;
            db.addController(controller);
            for (const sensor in entry) {
                if (sensor == "controller" || sensor == "timestamp") continue;
                db.addSensor(controller, sensor);
                const readings = entry[sensor];
                for (const name in readings) {
                    if (name == "_type") continue;
                    db.addSensorReading(controller, sensor, parseInt(timestamp), name, readings[name]);
                }
            }

        }



        return json({
            success: true,
            message: 'Sensor data processed successfully',
        });

    } catch (err) {
        console.error('Error processing sensor data:', err);

        if (err instanceof SyntaxError) {
            return error(400, {
                message: 'Invalid JSON in request body'
            });
        }

        return error(500, {
            message: 'Internal server error'
        });
    }
};
