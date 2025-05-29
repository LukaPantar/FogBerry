import * as db  from "$lib/database.server"

export async function load() {
    const controllers = db.getAllControllers();
    return { controllers }
}

