<script lang="ts">
    import { fade } from "svelte/transition";
    let { url, children } = $props();
</script>

<style>
    .transition {
        height: 100%;
    }

</style>


{#key url}
    <div class="transition" in:fade>
        {@render children?.()}
    </div>
{/key}