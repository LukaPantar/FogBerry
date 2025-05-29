import * as db  from "$lib/database.server"
import {getSensorReadingKeyCount} from "$lib/database.server";


export async function load({ params }) {
    const { controller } = params
    const sensors = db.getSensorsByController(controller)
    const firstReading = db.getControllerFirstReadingUnix(controller)
    const lastReading = db.getControllerLastReadingUnix(controller)

    let outSensors = []

    for (let sensor of sensors) {
        const count = getSensorReadingKeyCount(controller, sensor.name);
        const last20 = db.getSensorReadings(controller, sensor.name, 1, 20 * count);
        outSensors.push({ name: sensor.name, readings: last20.data });
    }
    return { controller, firstReading, lastReading, "sensors": outSensors };
}

