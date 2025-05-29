
<script lang="ts">

    import RaspberryPiIcon from "$lib/icons/RaspberryPiIcon.svelte";
    import {goto} from "$app/navigation";

    let { data } = $props();
    console.log(data)


</script>

<style>

    article {
        display: flex;
        flex-wrap: wrap;
        gap: 20px;
    }


    .card {
        flex: 1 1 300px;
        height: 76px;
        background-color: var(--bg-primary);
        border-radius: 15px;
        border: 3px solid var(--bg-secondary);
        padding: 12px;
        display: flex;
        align-items: center;
    }
    .card div.icon {
        padding: 6px;

    }

</style>

<article>

    {#each data.controllers as controller}
        <a class="card btn" href="/{controller.name}">
            <div class="icon">
                <RaspberryPiIcon></RaspberryPiIcon>
            </div>
            <div>
                <h3>{controller.name}</h3>
            </div>

        </a>
    {/each}
</article>
