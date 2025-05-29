import type {ServerLoad} from "@sveltejs/kit";

export const load: ServerLoad = async ({url}) => {
    return {
        url: url.pathname,
    };
}